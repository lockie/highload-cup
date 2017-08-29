#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <netinet/tcp.h>

#include <event2/event.h>

#include <cJSON.h>

#include "database.h"
#include "request.h"
#include "utils.h"
#include "cmdline.h"


int verbose;
int dump;
int phase_hack;

static void terminate_handler(int signum)
{
    fprintf(stderr, "Caught signal %d, terminating\n", signum);
    exit(signum);
}

static void setup_signals()
{
    VERIFY_NOT(signal(SIGPIPE, SIG_IGN), SIG_ERR);
    VERIFY_NOT(signal(SIGINT, terminate_handler), SIG_ERR);
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

typedef void* malloc_t(size_t);

// XXX really weird bug in SQLite memsys5: calling realloc(0, sz) causes SIGSEGV
void* memsys5_realloc(void* ptr, size_t sz)
{
    if(LIKELY(ptr != NULL))
        return sqlite3_realloc(ptr, sz);
    return sqlite3_malloc(sz);
}

static void setup_memory(size_t pool_size)
{
    void* pool;
    VERIFY_POSITIVE(pool = mmap(NULL, pool_size,
                  PROT_READ | PROT_WRITE,
                  /* private CoW memory */
                  MAP_PRIVATE | MAP_ANONYMOUS
                  /* no swapping, pre-fault */
                  | MAP_NORESERVE | MAP_POPULATE,
                  -1, 0));
    VERIFY_ZERO(madvise(pool, pool_size, MADV_DONTDUMP));
    /* XXX use hugepages to speed up page lookups */
    FILE* hugetlb = fopen("/sys/kernel/mm/transparent_hugepage/enabled", "r");
    if(!hugetlb)
    {
        if(verbose)
            printf("Hugepages not supported by OS!\n");
    }
    else
    {
        char status[32] = {0};
        size_t read = fread(status, 1, 32, hugetlb);
        if(strncmp(status, "[never]", read) == 0)
        {
            if(verbose)
                printf("Hugepages support turned off!\n");
        }
        else
        {
            VERIFY_ZERO(madvise(pool, pool_size, MADV_HUGEPAGE));
            if(verbose)
                printf("Hugepages support enabled\n");
        }
        fclose(hugetlb);
    }

    sqlite3_config(SQLITE_CONFIG_HEAP, pool, pool_size, 256);

    if(verbose)
        printf("Reserved %s for memory pool\n", print_size(pool_size));

    VERIFY_ZERO(sqlite3_config(SQLITE_CONFIG_LOOKASIDE, 0, 0));

    sqlite3_mem_methods sqlite_mm;
    sqlite3_config(SQLITE_CONFIG_GETMALLOC, &sqlite_mm);
    /* damn SQLite with damn int's */
    malloc_t* sqlite_malloc_fn = (malloc_t*)sqlite_mm.xMalloc;

    cJSON_Hooks cjson_mm;
    cjson_mm.malloc_fn = sqlite_malloc_fn;
    cjson_mm.free_fn = sqlite_mm.xFree;
    cJSON_InitHooks(&cjson_mm);

    event_set_mem_functions(sqlite_malloc_fn, memsys5_realloc, sqlite_mm.xFree);
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
    dump = args.dump_flag;
    if(dump)
        verbose = 0;
#ifdef NDEBUG
    if(dump)
    {
        fprintf(stderr, "This is release build, dump functionality disabled\n");
        return EXIT_FAILURE;
    }
#endif

    setup_signals();

    setup_memory(1L << args.memory_arg);

    database_t database;
    MEASURE_DURATION(VERIFY_ZERO(bootstrap(&database, args.data_arg)),
                     "Bootstrapping");

    if(dump)
        return EXIT_SUCCESS;

#ifndef NDEBUG
    if(verbose)
    {
        event_enable_debug_mode();
        event_enable_debug_logging(EVENT_DBG_ALL);
    }
#endif  /* NDEBUG */

    struct event_config* config;
    VERIFY_NONZERO(config = event_config_new());
    event_config_set_flag(config, EVENT_BASE_FLAG_NOLOCK);
    event_config_set_flag(config, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);
    struct event_base* base;
    VERIFY_NONZERO(base = event_base_new_with_config(config));
    event_config_free(config);
    struct evhttp* http;
    VERIFY_NONZERO(http = evhttp_new(base));
    evhttp_set_default_content_type(http, "application/json; charset=utf-8");
    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_POST);
    evhttp_set_gencb(http, request_handler, &database);
    struct evhttp_bound_socket* socket;
    VERIFY_NONZERO(socket = evhttp_bind_socket_with_handle(
                       http, "0.0.0.0", args.port_arg));
    int yes = 1;
    VERIFY_ZERO(setsockopt(evhttp_bound_socket_get_fd(socket),
                           IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)));
    if(verbose)
        printf("Listening on port %d using %s\n", args.port_arg,
               event_base_get_method(base));
    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
