#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <event2/event.h>
#include <event2/http.h>

#include <cJSON.h>

#include "database.h"
#include "request.h"


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
    ev_uint16_t port = 8080;

    (void)argc;
    (void)argv;

    setup_signals();

    cJSON_InitHooks(NULL);

    sqlite3* db;
    VERIFY_ZERO(sqlite3_open(":memory:", &db));
    VERIFY_ZERO(bootstrap(db));

#ifndef NDEBUG
    event_enable_debug_mode();
    event_enable_debug_logging(EVENT_DBG_ALL);
#endif  /* NDEBUG */

    struct event_base* base;
    VERIFY_NONZERO(base = event_base_new());
    struct evhttp* http;
    VERIFY_NONZERO(http = evhttp_new(base));
    evhttp_set_default_content_type(http, "application/json; charset=utf-8");
    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_POST);
    evhttp_set_gencb(http, request_handler, db);
    VERIFY_NONZERO(evhttp_bind_socket_with_handle(
                       http, "0.0.0.0", port));
    printf("Listening on port %d\n", port);
    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
