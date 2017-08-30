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
#include "methods.h"
#include "response.h"
#include "utils.h"
#include "request.h"


static int convert_int(const char* value, int* error)
{
    if(!value)
        return INT_MAX;
    if(!value[0])
    {
        *error = 1;
        return INT_MAX;
    }
    for(const char* c = value; *c; c++)
    {
        if(UNLIKELY(!isdigit(*c)))
        {
            *error = 1;
            return INT_MAX;
        }
    }
    return atoi(value);
}

void request_handler(struct evhttp_request* req, void* arg)
{
    int res;
    const char* URI = evhttp_request_get_uri(req);
    size_t i;

    /* figure entity from URL */
    int entity = -1;
    for(i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++)
    {
        if(strncmp(&URI[1], ENTITIES[i].name, ENTITIES[i].name_size) == 0)
        {
            entity = i;
            if(UNLIKELY(URI[ENTITIES[i].name_size+1] != 47)) /* / */
                entity = -1;
            break;
        }
    }
    if(UNLIKELY(entity == -1))
    {
        handle_bad_request(req, "invalid entity");
        return;
    }

    /* figure operation */
    int write = (evhttp_request_get_command(req) == EVHTTP_REQ_POST);

    /* figure id */
    int id;
    size_t n = ENTITIES[i].name_size + 2;
    size_t nURI = strlen(&URI[n]);
    char* identifier = alloca(nURI + 1);
    strncpy(identifier, &URI[n], nURI + 1);
    if(UNLIKELY(write && strncmp(identifier, "new", 3) == 0))
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
            if(UNLIKELY(!isdigit(identifier[i])))
            {
                /* handle_bad_request(req, "invalid id"); */
                handle_not_found(req); /* yeah sure */
                return;
            }
        }
        if(UNLIKELY(i == 0))
        {
            handle_bad_request(req, "missing id");
            return;
        }
        n += i;
        id = atoi(identifier);
    }

    /* figure out remainder */
    int method = METHOD_DEFAULT;
    if(URI[n] == '/')
    {
        const char* method_str = &URI[++n];
        for(i = 0; i < sizeof(METHODS) / sizeof(METHODS[0]); i++)
        {
            if(strncmp(method_str, METHODS[i], METHODS_SIZE[i]) == 0)
            {
                method = i;
                n += METHODS_SIZE[i];
                if(UNLIKELY(URI[n] && URI[n] != '?'))
                {
                    handle_bad_request(req, "invalid query string");
                    return;
                }
                break;
            }
        }
    }

    /* figure query parameters */
    parameters_t params = {INT_MAX, INT_MAX, {0}, INT_MAX, INT_MAX, INT_MAX, 0};
    if(URI[n] == '?')
    {
        if(method != METHOD_DEFAULT)
        {
            struct evkeyvalq query;
            CHECK_POSITIVE(evhttp_parse_query_str(&URI[n+1], &query));

            const char* gender = evhttp_find_header(&query, "gender");
            if(gender && gender[1] != 0)
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
                if(UNLIKELY(e)) goto error;
                params.toDate = convert_int(
                    evhttp_find_header(&query, "toDate"), &e);
                if(UNLIKELY(e)) goto error;

                const char* country =
                    evhttp_find_header(&query, "country");
                if(UNLIKELY(country && country[0] == 0))
                    goto error;
                if(country)
                    strncpy(params.country, country, 64);
                params.toDistance = convert_int(
                    evhttp_find_header(&query, "toDistance"), &e);
                if(UNLIKELY(e)) goto error;

                params.fromAge = convert_int(
                    evhttp_find_header(&query, "fromAge"), &e);
                if(UNLIKELY(e)) goto error;
                params.toAge = convert_int(
                    evhttp_find_header(&query, "toAge"), &e);
                if(UNLIKELY(e)) goto error;

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
        if(UNLIKELY(URI[n]))
        {
            handle_bad_request(req, "invalid query string tail");
            return;
        }
    }

    /* get POST body, if any */
    char* body = NULL;
    if(UNLIKELY(write))
    {
        struct evbuffer* in_buf = evhttp_request_get_input_buffer(req);
        size_t length = evbuffer_get_length(in_buf);
        body = alloca(length);
        CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));
    }

    /* do processing */
    char response[RESPONSE_BUFFER_SIZE];
    if(method == METHOD_DEFAULT)
        res = process_entity(arg, entity, id, write, body, response);
    else
        res = execute_method(arg, entity, id, method, &params, response);

    switch(res)
    {
    case PROCESS_RESULT_OK:
    {
        struct evbuffer* out_buf;
        CHECK_NONZERO(out_buf = evbuffer_new());
        evbuffer_add(out_buf, response, strlen(response));
        evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
        evbuffer_free(out_buf);
        break;
    }

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

    return;

cleanup:
    evhttp_connection_free(req->evcon);
}
