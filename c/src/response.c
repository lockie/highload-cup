#include <string.h>

#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/http_struct.h>

#include "utils.h"
#include "response.h"


static const char* ERR_FORMAT = "{\"error\":\"%s\"}";

void handle_bad_request(struct evhttp_request* req, const char* msg)
{
    struct evbuffer* buf;
    CHECK_NONZERO(buf = evbuffer_new());

    CHECK_POSITIVE(
        evbuffer_add_printf(buf, ERR_FORMAT, msg ? msg : "unknown error"));

    evhttp_send_reply(req, HTTP_BADREQUEST, "Bad Request", buf);

    evbuffer_free(buf);
    return;

cleanup:
    if(LIKELY(buf))
        evbuffer_free(buf);
    evhttp_connection_free(req->evcon);
}

void handle_not_found(struct evhttp_request* req)
{
    struct evbuffer* buf;
    CHECK_NONZERO(buf = evbuffer_new());

    static const char* message = "{\"error\":\"resource not found\"}";
    CHECK_POSITIVE(evbuffer_add(buf, message, strlen(message)));

    evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", buf);

    evbuffer_free(buf);
    return;

cleanup:
    if(LIKELY(buf))
        evbuffer_free(buf);
    evhttp_connection_free(req->evcon);
}
