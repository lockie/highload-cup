#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>


/* I have no idea what I'm doing.png */
/* https://stackoverflow.com/a/10119699/1336774 */
#define IS_SET(macro) IS_SET_(macro)
#define MACROTEST_1 ,
#define IS_SET_(value) IS_SET__(MACROTEST_##value)
#define IS_SET__(comma) IS_SET___(comma 1, 0)
#define IS_SET___(_, v, ...) v

#define VERIFY_NOT(x, err) if(x == err){perror(#x); exit(EXIT_FAILURE);}
#define VERIFY_ZERO(x) {int rc = x; if(rc!=0){perror(#x); exit(rc);}}
#define CHECK_POSITIVE(x) {int rc = (x); if(rc<0) {           \
            if(!(IS_SET(NDEBUG)))                             \
                fprintf(stderr, "%s failed (%d).\n", #x, rc);    \
            goto cleanup;}}
#define CHECK_NONZERO(x) {if(!(x)) {                 \
            if(!(IS_SET(NDEBUG)))                    \
                fprintf(stderr, "%s failed.\n", #x); \
            goto cleanup;}}


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

static const char* ERR_FORMAT = "{\"error\": \"%s\"}";

static void handle_bad_request(struct evhttp_request* req, const char* msg)
{
    struct evbuffer* buf = evbuffer_new();
    if(!buf)
        goto cleanup;

    CHECK_POSITIVE(
        evbuffer_add_printf(buf, ERR_FORMAT, msg ? msg : "unknown error"));

    evhttp_send_reply(req, HTTP_BADREQUEST, "Bad Request", buf);

    evbuffer_free(buf);
    return;

cleanup:
    if(buf)
        evbuffer_free(buf);
    evhttp_connection_free(req->evcon);
}

static void handle_not_found(struct evhttp_request* req)
{
    struct evbuffer* buf = evbuffer_new();
    if(!buf)
        goto cleanup;

    static const char* message = "resource not found";
    CHECK_POSITIVE(evbuffer_add(buf, message, strlen(message)));

    evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", buf);

    evbuffer_free(buf);
    return;

cleanup:
    if(buf)
        evbuffer_free(buf);
    evhttp_connection_free(req->evcon);
}


/* TODO : refactor to multiple files?.. */

#define PROCESS_RESULT_OK 0
#define PROCESS_RESULT_BAD_REQUEST 1
#define PROCESS_RESULT_NOT_FOUND 2

static int process_entity(const char* entity, int write, const char* body, char** response)
{
    if(write)
    {
        /* TODO : actual writing */
        *response = strdup("{}");
        return PROCESS_RESULT_OK;
    }

    /* TODO : actual reading */
    *response = strdup(entity);
    return PROCESS_RESULT_OK;
}

static const char* ENTITIES[3] = {"users", "visits", "locations"};

static void request_handler(struct evhttp_request* req, void* arg)
{
    struct evbuffer* in_buf, *out_buf;
    const char* URI = evhttp_request_get_uri(req);
    const char* entity = NULL;
    char* body = NULL;
    char* response;
    int write = 0, res;
    size_t i;

    /* figure entity from URL */
    for(i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
    {
        if(strncmp(&URI[1], ENTITIES[i], strlen(ENTITIES[i])) == 0)
        {
            entity = ENTITIES[i];
            if(URI[strlen(entity)+1] != 47) /* / */
                entity = NULL;
            break;
        }
    }
    if(!entity)
    {
        handle_bad_request(req, "invalid entity");
        return;
    }

    /* figure operation */
    switch (evhttp_request_get_command(req))
    {
    case EVHTTP_REQ_GET:
        write = 0;
        break;
    case EVHTTP_REQ_POST:
        write = 1;
        break;
    default:
        goto cleanup;
        break;
    }
    if(write)
    {
        in_buf = evhttp_request_get_input_buffer(req);
        size_t length = evbuffer_get_length(in_buf);
        CHECK_NONZERO(body = malloc(length))
        CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));
    }

    /* do processing */
    res = process_entity(entity, write, body, &response);
    switch(res)
    {
    case PROCESS_RESULT_OK:
        out_buf = evbuffer_new();
        evbuffer_add(out_buf, response, strlen(response));
        evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
        break;

    case PROCESS_RESULT_BAD_REQUEST:
        handle_bad_request(req, response);
        break;

    case PROCESS_RESULT_NOT_FOUND:
        handle_not_found(req);
        break;

    default:
        goto cleanup;
    }

    return;

cleanup:
    evhttp_connection_free(req->evcon);
}

int main(int argc, char** argv)
{
    struct event_base* base;
    struct evhttp* http;
    struct evhttp_bound_socket* handle;
    ev_uint16_t port = 8080;

    (void)argc;
    (void)argv;

    setup_signals();

#ifndef NDEBUG
    event_enable_debug_mode();
    event_enable_debug_logging(EVENT_DBG_ALL);
#endif  /* NDEBUG */

    base = event_base_new();
    if(!base)
        return EXIT_FAILURE;

    http = evhttp_new(base);
    if(!http)
        return EXIT_FAILURE;

    evhttp_set_default_content_type(http, "application/json; charset=utf-8");
    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_POST);

    evhttp_set_gencb(http, request_handler, NULL);

    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
    if (!handle)
        return EXIT_FAILURE;

    printf("Listening on port %d\n", port);

    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
