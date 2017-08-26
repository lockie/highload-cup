#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <alloca.h>
#include <signal.h>
#include <limits.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

#include <cJSON.h>

#include <sqlite3.h>

#include <miniz_zip.h>


/* I have no idea what I'm doing.png */
/* https://stackoverflow.com/a/10119699/1336774 */
#define IS_SET(macro) IS_SET_(macro)
#define MACROTEST_1 ,
#define IS_SET_(value) IS_SET__(MACROTEST_##value)
#define IS_SET__(comma) IS_SET___(comma 1, 0)
#define IS_SET___(_, v, ...) v

#define VERIFY_NOT(x, err) if(x == err){perror(#x); exit(EXIT_FAILURE);}
#define VERIFY_ZERO(x) {int rc = (x); if(rc!=0){perror(#x); exit(rc);}}
#define VERIFY_NONZERO(x) {if(!(x)){perror(#x); exit(EXIT_FAILURE);}}
#define CHECK_POSITIVE(x) {int rc = (x); if(rc<0) {           \
            if(!(IS_SET(NDEBUG)))                             \
                fprintf(stderr, "%s failed (%d).\n", #x, rc);    \
            goto cleanup;}}
#define CHECK_NONZERO(x) {if(!(x)) {                 \
            if(!(IS_SET(NDEBUG)))                    \
                fprintf(stderr, "%s failed.\n", #x); \
            goto cleanup;}}
#define CHECK_SQL(x) {rc=(x); if(rc!=0&&rc!=SQLITE_DONE){ \
            if(!(IS_SET(NDEBUG)))                         \
                fprintf(stderr, "%s: SQL error %d: %s\n", \
                        #x, rc, sqlite3_errstr(rc));      \
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

#define METHOD_DEFAULT -1
#define METHOD_AVG  0
#define METHOD_VISITS 1

#define PROCESS_RESULT_OK 0
#define PROCESS_RESULT_BAD_REQUEST 1
#define PROCESS_RESULT_NOT_FOUND 2
#define PROCESS_RESULT_ERROR 3

struct parameters_t
{
    int fromDate;
    int toDate;

    const char* country;
    int toDistance;

    int fromAge;
    int toAge;
    char gender;
};

int convert_int(const char* value, int* error)
{
    if(!value)
        return INT_MAX;
    for(size_t i = 0; i < strlen(value); i++)
    {
        if(!isdigit(value[i]))
        {
            *error = 1;
            return INT_MAX;
        }
    }
    return atoi(value);
}

static int process_entity(const char* entity, int id, int method, int write,
                          const struct parameters_t* parameters,
                          const char* body, char** response)
{
    if(write)
    {
        cJSON* root = cJSON_Parse(body);
        if(!root)
        {
            *response = strdup("failed to parse JSON");
            return PROCESS_RESULT_ERROR;
        }

        if(id == -1)
        {
            cJSON* json_id = cJSON_GetObjectItemCaseSensitive(root, "id");
            if(!json_id || !cJSON_IsNumber(json_id))
            {
                *response = strdup("failed to get id");
                return PROCESS_RESULT_BAD_REQUEST;
            }
        }

        /* TODO : actual writing */

        cJSON_Delete(root);

        *response = strdup("{}");
        return PROCESS_RESULT_OK;
    }

    /* TODO : actual reading */
    *response = strdup(entity);
    return PROCESS_RESULT_OK;
}

static const char* ENTITIES[3] = {"users", "visits", "locations"};

static const char* METHODS[2] = {"avg", "visits"};

static int SQL_callback(void *data, int argc, char **argv, char **azColName)
{
    struct evbuffer* out_buf = (struct evbuffer*)data;
    for(int i = 0; i<argc; i++)
    {
        evbuffer_add_printf(out_buf, "%s = %s\n",
                            azColName[i], argv[i] ? argv[i] : "NULL");
    }
    return 0;
}

static void request_handler(struct evhttp_request* req, void* arg)
{
    struct evbuffer* in_buf, *out_buf;
    const char* URI = evhttp_request_get_uri(req);
    struct evkeyvalq query;
    const char* entity = NULL;
    const char* method_str;
    struct parameters_t params;
    char* identifier;
    char* body = NULL;
    char* response;
    int write = 0, res, id, method = METHOD_DEFAULT;
    size_t i, n;

#ifndef NDEBUG
    /* XXX debug URL to peep into database */
    if(strncmp(URI, "/SQL", 4) == 0)
    {
        int rc;

        in_buf = evhttp_request_get_input_buffer(req);
        size_t length = evbuffer_get_length(in_buf);
        body = alloca(length);
        CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));

        out_buf = evbuffer_new();
        sqlite3* db = (sqlite3*)arg;
        CHECK_SQL(sqlite3_exec(db, body, SQL_callback, out_buf, NULL));
        evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
        evbuffer_free(out_buf);
        return;
    }

#endif // ifndef NDEBUG

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

    /* figure id */
    n = strlen(entity) + 2;
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
    if(URI[n])
    {
        if(write)
        {
            handle_bad_request(req, "invalid query string for POST");
            return;
        }
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
    }

    /* figure query parameters */
    if(URI[n] == '?')
    {
        CHECK_POSITIVE(evhttp_parse_query_str(&URI[n+1], &query));
        if(method != METHOD_DEFAULT)
        {
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

                params.country =
                    evhttp_find_header(&query, "country");
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
                handle_bad_request(req, "invalid integer query parameter");
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
    res = process_entity(entity, id, method, write, &params, body, &response);
    switch(res)
    {
    case PROCESS_RESULT_OK:
        out_buf = evbuffer_new();
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
    evhttp_connection_free(req->evcon);
}

static const char* DDL = R"(
CREATE TABLE users (
    id INTEGER NOT NULL,
    email VARCHAR(100),
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    gender CHAR(1),
    birth_date INTEGER,
    PRIMARY KEY (id)
);

CREATE TABLE locations (
    id INTEGER NOT NULL,
    place VARCHAR,
    country VARCHAR(50),
    city VARCHAR(50),
    distance INTEGER,
    PRIMARY KEY (id)
);

CREATE TABLE visits (
    id INTEGER NOT NULL,
    location INTEGER,
    user INTEGER,
    visited_at INTEGER,
    mark SMALLINT,
    PRIMARY KEY (id),
    FOREIGN KEY(location) REFERENCES locations (id),
    FOREIGN KEY(user) REFERENCES users (id)
);
)";

static const char* INSERT_USER = R"(
INSERT INTO users (id, email, first_name, last_name, gender, birth_date)
VALUES (?, ?, ?, ?, ?, ?);
)";

int bootstrap(sqlite3* db)
{
    int rc = 0;

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));

    CHECK_SQL(sqlite3_exec(db, DDL, NULL, NULL, NULL));
    CHECK_SQL(sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL));

    sqlite3_stmt* insert_user;
    CHECK_SQL(sqlite3_prepare_v3(
                    db,
                    INSERT_USER,
                    strlen(INSERT_USER),
                    SQLITE_PREPARE_PERSISTENT,
                    &insert_user,
                    NULL));

    if(!mz_zip_reader_init_file(&archive, "/tmp/data/data.zip", 0))
    {
        return mz_zip_get_last_error(&archive);
    }
    for(unsigned i = 0; i < mz_zip_reader_get_num_files(&archive); i++)
    {
        mz_zip_archive_file_stat stat;
        if(!mz_zip_reader_file_stat(&archive, i, &stat))
        {
            mz_zip_reader_end(&archive);
            return mz_zip_get_last_error(&archive);
        }
#ifndef NDEBUG
        printf("Processing %s...\n", stat.m_filename);
#endif  // !NDEBUG

        char* data = mz_zip_reader_extract_to_heap(&archive, i, NULL, 0);
        if (!data)
        {
            // limp on
            fprintf(stderr, "Processing file '%s' in zip archive FAILED!\n",
                    stat.m_filename);
            continue;
        }

        cJSON* root = cJSON_Parse(data);
        if(root)
        {
            cJSON* users = cJSON_GetObjectItemCaseSensitive(root, "users");
            if(users)
            {
                // XXX omitting checks, assuming inputs are correct
                for(cJSON* user = users->child; user; user = user->next)
                {
                    CHECK_SQL(sqlite3_bind_int(
                                  insert_user, 1,
                                  cJSON_GetObjectItemCaseSensitive(user, "id")->valueint));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 2,
                                  cJSON_GetObjectItemCaseSensitive(user, "email")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 3,
                                  cJSON_GetObjectItemCaseSensitive(user, "first_name")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 4,
                                  cJSON_GetObjectItemCaseSensitive(user, "last_name")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 5,
                                  cJSON_GetObjectItemCaseSensitive(user, "gender")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_int(
                                  insert_user, 6,
                                  cJSON_GetObjectItemCaseSensitive(user, "birth_date")->valueint));

                    CHECK_SQL(sqlite3_step(insert_user));
                    CHECK_SQL(sqlite3_reset(insert_user));
                }
            }
        }
        else
        {
            // limp on
            fprintf(stderr, "Parsing file '%s' in zip archive FAILED!\n",
                    stat.m_filename);
        }

        cJSON_Delete(root);
        mz_free(data);
    }

cleanup:
    mz_zip_reader_end(&archive);
    if(rc == 0)
    {
        CHECK_SQL(sqlite3_finalize(insert_user));
        CHECK_SQL(sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL));
    }
    return rc;
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

    cJSON_InitHooks(NULL);

    sqlite3* db;
    VERIFY_ZERO(sqlite3_open(":memory:", &db));

    VERIFY_ZERO(bootstrap(db));


#ifndef NDEBUG
    event_enable_debug_mode();
    event_enable_debug_logging(EVENT_DBG_ALL);
#endif  /* NDEBUG */

    VERIFY_NONZERO(base = event_base_new());

    VERIFY_NONZERO(http = evhttp_new(base));

    evhttp_set_default_content_type(http, "application/json; charset=utf-8");
    evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_POST);

    evhttp_set_gencb(http, request_handler, db);

    VERIFY_NONZERO(handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port));

    printf("Listening on port %d\n", port);

    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
