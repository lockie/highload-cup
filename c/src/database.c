#include <miniz_zip.h>
#include <cJSON.h>
#include <event2/buffer.h>

#include "database.h"


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
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "id")->valueint));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 2,
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "email")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 3,
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "first_name")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 4,
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "last_name")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_text(
                                  insert_user, 5,
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "gender")->valuestring,
                                  -1, SQLITE_TRANSIENT));
                    CHECK_SQL(sqlite3_bind_int(
                                  insert_user, 6,
                                  cJSON_GetObjectItemCaseSensitive(
                                      user, "birth_date")->valueint));

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

static int SQL_callback(void *data, int argc, char **argv, char **azColName)
{
    for(int i = 0; i<argc; i++)
    {
        evbuffer_add_printf((struct evbuffer*)data, "%s = %s\n",
                            azColName[i], argv[i] ? argv[i] : "NULL");
    }
    return 0;
}

int process_SQL(struct evhttp_request* req, void* arg)
{
    int rc = -1;
    struct evbuffer* in_buf, *out_buf = NULL;

    CHECK_NONZERO(in_buf = evhttp_request_get_input_buffer(req));
    size_t length = evbuffer_get_length(in_buf);
    char* body = alloca(length);
    CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));

    CHECK_NONZERO(out_buf = evbuffer_new());
    sqlite3* db = (sqlite3*)arg;
    CHECK_SQL(sqlite3_exec(db, body, SQL_callback, out_buf, NULL));
    evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
    rc = 0;

cleanup:
    if(out_buf)
        evbuffer_free(out_buf);
    return rc;
}
