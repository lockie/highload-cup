#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/keyvalq_struct.h>

#include "database.h"
#include "entity.h"
#include "response.h"
#include "utils.h"
#include "request.h"


static int convert_int(const char* value, int* error)
{
    if(!value)
        return INT_MAX;
    size_t len = strlen(value);
    if(len == 0)
    {
        *error = 1;
        return INT_MAX;
    }
    for(size_t i = 0; i < len; i++)
    {
        if(!isdigit(value[i]))
        {
            *error = 1;
            return INT_MAX;
        }
    }
    return atoi(value);
}

static const char* METHODS[2] = {"avg", "visits"};

void request_handler(struct evhttp_request* req, void* arg)
{
    struct evbuffer* in_buf, *out_buf;
    const char* URI = evhttp_request_get_uri(req);
    struct evkeyvalq query;
    int entity = -1;
    const char* method_str;
    char* identifier;
    char* body = NULL;
    char* response = NULL;
    int write = 0, res, id, method = METHOD_DEFAULT;
    size_t i, n;

#ifndef NDEBUG
    /* XXX debug URL to peep into database */
    if(strncmp(URI, "/SQL", 4) == 0)
    {
        int rc = process_SQL(req, arg);
        if(rc != 0)
        {
            handle_bad_request(req, sqlite3_errstr(rc));
        }
        return;
    }
#endif // ifndef NDEBUG

    /* figure entity from URL */
    for(i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
    {
        if(strncmp(&URI[1], ENTITIES[i].name, strlen(ENTITIES[i].name)) == 0)
        {
            entity = i;
            if(URI[strlen(ENTITIES[i].name)+1] != 47) /* / */
                entity = -1;
            break;
        }
    }
    if(entity == -1)
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

    /* figure id */
    n = strlen(ENTITIES[i].name) + 2;
    identifier = alloca(strlen(URI) - n + 1);
    strncpy(identifier, &URI[n], strlen(URI) - n + 1);
    if(write && strncmp(identifier, "new", 3) == 0)
    {
        n += 3;
        id = -1;
    }
    else
    {
        for(i = 0; identifier[i]; i++)
        {
            if(identifier[i] == '?' || identifier[i] == '/')
            {
                identifier[i] = 0;
                break;
            }
            if(!isdigit(identifier[i]))
            {
                /* handle_bad_request(req, "invalid id"); */
                handle_not_found(req); /* yeah sure */
                return;
            }
        }
        if(i == 0)
        {
            handle_bad_request(req, "missing id");
            return;
        }
        n += i;
        id = atoi(identifier);
    }

    /* figure out remainder */
    if(URI[n] == '/')
    {
        method_str = &URI[++n];
        for(i = 0; i < sizeof(METHODS) / sizeof(METHODS[0]); i++)
        {
            if(strncmp(method_str, METHODS[i], strlen(METHODS[i])) == 0)
            {
                method = i;
                n += strlen(METHODS[i]);
                if(URI[n] && URI[n] != '?')
                {
                    handle_bad_request(req, "invalid query string");
                    return;
                }
                break;
            }
        }
    }

    /* figure query parameters */
    parameters_t params = {INT_MAX, INT_MAX, NULL, INT_MAX, INT_MAX, INT_MAX, 0};
    if(URI[n] == '?')
    {
        if(method != METHOD_DEFAULT)
        {
            CHECK_POSITIVE(evhttp_parse_query_str(&URI[n+1], &query));

            const char* gender = evhttp_find_header(&query, "toAge");
            if(gender && strlen(gender) != 2)
            {
                evhttp_clear_headers(&query);
                handle_bad_request(req, "invalid gender parameter value");
                return;
            }

            for(;;)
            {
                int e = 0;

                params.fromDate = convert_int(
                    evhttp_find_header(&query, "fromDate"), &e);
                if(e) goto error;
                params.toDate = convert_int(
                    evhttp_find_header(&query, "toDate"), &e);
                if(e) goto error;

                const char* country =
                    evhttp_find_header(&query, "country");
                if(country && country[0] == 0)
                    goto error;
                if(country)
                    params.country = strdup(country);
                params.toDistance = convert_int(
                    evhttp_find_header(&query, "toDistance"), &e);
                if(e) goto error;

                params.fromAge = convert_int(
                    evhttp_find_header(&query, "fromAge"), &e);
                if(e) goto error;
                params.toAge = convert_int(
                    evhttp_find_header(&query, "toAge"), &e);
                if(e) goto error;
                params.gender = gender ? gender[0] : 0;

                evhttp_clear_headers(&query);
                break;

error:
                evhttp_clear_headers(&query);
                handle_bad_request(req, "invalid query parameter");
                return;
            }
        }
    }
    else
    {
        if(URI[n])
        {
            handle_bad_request(req, "invalid query string tail");
            return;
        }
    }

    /* get POST body, if any */
    if(write)
    {
        in_buf = evhttp_request_get_input_buffer(req);
        size_t length = evbuffer_get_length(in_buf);
        body = alloca(length);
        CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));
    }

    /* do processing */
    res = process_entity(arg, entity, id, method, write, &params, body,
                         &response);

    switch(res)
    {
    case PROCESS_RESULT_OK:
        CHECK_NONZERO(out_buf = evbuffer_new());
        evbuffer_add(out_buf, response, strlen(response));
        evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
        evbuffer_free(out_buf);
        break;

    case PROCESS_RESULT_BAD_REQUEST:
        handle_bad_request(req, response);
        break;

    case PROCESS_RESULT_NOT_FOUND:
        handle_not_found(req);
        break;

    case PROCESS_RESULT_ERROR:
    default:
        goto cleanup;
    }

    free(response);
    return;

cleanup:
    free(params.country);
    evhttp_connection_free(req->evcon);
}
