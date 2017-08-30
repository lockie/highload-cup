#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netinet/tcp.h>

#include <pthread.h>

#include <jemalloc/jemalloc.h>

#include <event2/event.h>
#include <event2/thread.h>

#include <cJSON.h>

#include "database.h"
#include "request.h"
#include "utils.h"
#include "cmdline.h"


int verbose;
int phase_hack;

static void terminate_handler(int signum)
{
    fprintf(stderr, "\nCaught signal %d, terminating\n", signum);
    exit(signum);
}

static void hangup_handler(int signum)
{
    (void)signum;
    malloc_stats_print(NULL, NULL, NULL);
}

static void setup_signals()
{
    VERIFY_NOT(signal(SIGPIPE, SIG_IGN), SIG_ERR);
    VERIFY_NOT(signal(SIGINT, terminate_handler), SIG_ERR);
    VERIFY_NOT(signal(SIGHUP, hangup_handler), SIG_ERR);
}

// based on https://stackoverflow.com/a/3898986/1336774
const char* print_size(uint64_t size)
{
    uint64_t  multiplier = 1024ULL * 1024ULL * 1024ULL;
    static const char* sizes[] = {"GiB", "MiB", "KiB", "B"};
    static char result[32] = {0};
    for(unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]);
        i++, multiplier /= 1024)
    {
        if(size < multiplier)
            continue;
        if(size % multiplier == 0)
            snprintf(result, 32, "%" PRIu64 " %s", size / multiplier, sizes[i]);
        else
            snprintf(result, 32, "%.1f %s", (float) size / multiplier, sizes[i]);
        return result;
    }
    return result;
}


static void setup_memory()
{
    if(mlockall(MCL_CURRENT) != 0)
    {
        perror("mlockall");
    }
}

static int setup_socket(int portnum)
{
    int fd;
    VERIFY_POSITIVE(fd = socket(AF_INET, SOCK_STREAM, 0));

    int yes = 1;
    VERIFY_ZERO(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                           &yes, sizeof(int)));
    VERIFY_ZERO(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                           &yes, sizeof(int)));
    VERIFY_ZERO(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                           &yes, sizeof(int)));


    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(portnum);

    VERIFY_POSITIVE(bind(fd, (struct sockaddr*)&addr, sizeof(addr)));
    VERIFY_POSITIVE(listen(fd, 2048));

    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0
        || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    return fd;
}

void* thread_dispatch(void* arg)
{
    event_base_dispatch((struct event_base*)arg);
    return NULL;
}

static void setup_threads(int count, int portnum, database_t* database)
{
    VERIFY_ZERO(evthread_use_pthreads());

    int fd = setup_socket(portnum);
    pthread_t* threads = alloca(count * sizeof(pthread_t));
    const char* method = NULL;
    for(int i = 0; i < count; i++)
    {
        struct event_config* config;
        VERIFY_NONZERO(config = event_config_new());
        if(count == 1)
            event_config_set_flag(config, EVENT_BASE_FLAG_NOLOCK);
        event_config_set_flag(config, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);

        struct event_base* base;
        VERIFY_NONZERO(base = event_base_new_with_config(config));
        event_config_free(config);

        if(verbose && i == 0)
            method = event_base_get_method(base);

        struct evhttp* httpd;
        VERIFY_NONZERO(httpd = evhttp_new(base));
        evhttp_set_default_content_type(httpd, "application/json; charset=utf-8");
        evhttp_set_allowed_methods(httpd, EVHTTP_REQ_GET | EVHTTP_REQ_POST);
        evhttp_set_gencb(httpd, request_handler, database);
        VERIFY_ZERO(evhttp_accept_socket(httpd, fd));

        VERIFY_ZERO(pthread_create(&threads[i], NULL, thread_dispatch, base));
    }

    if(verbose)
        printf("Listening on port %d using %s in %d thread(s)\n",
               portnum, method, count);

    for(int i = 0; i < count; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

int main(int argc, char** argv)
{
    // Docker <_<
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    struct gengetopt_args_info args;
    if(cmdline_parser(argc, argv, &args) != 0)
        return EXIT_FAILURE;

    phase_hack = args.phase_hack_flag;
    verbose = args.verbose_flag;
    if(args.threads_arg != 1 && args.phase_hack_flag)
    {
        fprintf(stderr, "Can't do phase hack in multithreaded mode!\n");
        return EXIT_FAILURE;
    }

    setup_signals();

    setup_memory();

    database_t database;
    MEASURE_DURATION(VERIFY_ZERO(bootstrap(&database, args.data_arg)),
                     "Bootstrapping");

#ifndef NDEBUG
    if(verbose)
    {
        event_enable_debug_mode();
        event_enable_debug_logging(EVENT_DBG_ALL);
    }
#endif  /* NDEBUG */

    setup_threads(args.threads_arg, args.port_arg, &database);

    return EXIT_SUCCESS;
}
