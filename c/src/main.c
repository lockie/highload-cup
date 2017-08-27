#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <event2/event.h>

#include <cJSON.h>

#include "database.h"
#include "request.h"
#include "utils.h"
#include "cmdline.h"


int verbose;
int dump;

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

int main(int argc, char** argv)
{
    // Docker <_<
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    struct gengetopt_args_info args;
    if(cmdline_parser(argc, argv, &args) != 0)
        return EXIT_FAILURE;

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

    cJSON_InitHooks(NULL);

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

    struct event_base* base;
    VERIFY_NONZERO(base = event_base_new());
    struct evhttp* http;
    VERIFY_NONZERO(http = evhttp_new(base));
    evhttp_set_default_content_type(http, "application/json; charset=utf-8");
    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_POST);
    evhttp_set_gencb(http, request_handler, &database);
    VERIFY_NONZERO(evhttp_bind_socket_with_handle(
                       http, "0.0.0.0", args.port_arg));
    if(verbose)
        printf("Listening on port %d\n", args.port_arg);
    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
