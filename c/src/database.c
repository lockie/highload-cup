#include <miniz_zip.h>
#include <event2/buffer.h>

#include "entity.h"
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
CREATE INDEX visits_location ON visits (location);
CREATE INDEX visits_user ON visits(user);
)";

static const char* INSERT_USER = R"(
INSERT INTO users (id, email, first_name, last_name, gender, birth_date)
VALUES (?, ?, ?, ?, ?, ?)
)";

static const char* INSERT_VISIT = R"(
INSERT INTO visits (id, location, user, visited_at, mark)
VALUES (?, ?, ?, ?, ?)
)";

static const char* INSERT_LOCATION = R"(
INSERT INTO locations (id, place, country, city, distance)
VALUES (?, ?, ?, ?, ?)
)";

static const char* SELECT_USER = R"(
SELECT id, email, first_name, last_name, gender, birth_date
FROM users WHERE id = ?
)";

static const char* SELECT_VISIT = R"(
SELECT id, location, user, visited_at, mark
FROM visits WHERE id = ?
)";

static const char* SELECT_LOCATION = R"(
SELECT id, place, country, city, distance
FROM locations WHERE id = ?
)";

static const char* UPDATE_USER = R"(
UPDATE users SET email=?2, first_name=?3, last_name=?4, gender=?5, birth_date=?6
WHERE id = ?1
)";

static const char* UPDATE_VISIT = R"(
UPDATE visits SET location=?2, user=?3, visited_at=?4, mark=?5
WHERE id = ?1
)";

static const char* UPDATE_LOCATION = R"(
UPDATE locations SET place=?2, country=?3, city=?4, distance=?5
WHERE id = ?1
)";

static inline int bind_val(sqlite3_stmt* stmt, const entity_t* entity,
                           cJSON* json, int i)
{
    if(entity->column_types[i] == COLUMN_TYPE_NONE)
        return 0;

    int rc;
    const void* value;

    cJSON* arg = cJSON_GetObjectItemCaseSensitive(json, entity->column_names[i]);
    if(!arg || cJSON_IsNull(arg))
        return PROCESS_RESULT_BAD_REQUEST;

    if(entity->column_types[i] == COLUMN_TYPE_INT)
    {
        /*
        if(!cJSON_IsInt(arg))
            return PROCESS_RESULT_BAD_REQUEST;
        */
        value = (void*)(intptr_t)arg->valueint;
    }
    else
    {
        /*
        if(!cJSON_IsString(arg))
            return PROCESS_RESULT_BAD_REQUEST;
        */
        value = arg->valuestring;
    }

    if(entity->column_types[i] == COLUMN_TYPE_INT)
    {
#ifndef NDEBUG
        if(dump)
            printf("%d", (int)(intptr_t)value);
#endif  // NDEBUG
        CHECK_SQL(sqlite3_bind_int(stmt, i+2, (intptr_t)value));
    }
    else
    {
#ifndef NDEBUG
        if(dump)
            printf("\"%s\"", (char*)value);
#endif  // NDEBUG
        CHECK_SQL(sqlite3_bind_text(stmt, i+2, value, -1, SQLITE_TRANSIENT));
    }

cleanup:
    return rc;
}

int insert_entity(database_t* database, cJSON* json, int e)
{
    int rc;
    const entity_t* entity = &ENTITIES[e];
    sqlite3_stmt* insert_stmt = database->create_stmts[e];
    CHECK_SQL(sqlite3_reset(insert_stmt));
    cJSON* id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if(!id || !cJSON_IsNumber(id))
        return PROCESS_RESULT_BAD_REQUEST;
#ifndef NDEBUG
    if(dump)
        printf("INSERT INTO %s VALUES (%d,", entity->name, id->valueint);
#endif  // NDEBUG
    CHECK_SQL(sqlite3_bind_int(insert_stmt, 1, id->valueint));
    for(int i = 0; i < 5; i++)
    {
        CHECK_ZERO(bind_val(insert_stmt, entity, json, i));
#ifndef NDEBUG
        if(dump)
        {
            if(i != 4 && entity->column_types[i+1] != COLUMN_TYPE_NONE)
                printf(",");
        }
#endif  // NDEBUG
    }
#ifndef NDEBUG
    if(dump)
        printf(");\n");
#endif  // NDEBUG
    CHECK_SQL(sqlite3_step(insert_stmt));
    if(rc == SQLITE_DONE)
        rc = 0;

cleanup:
    return rc;
}

static int insert_entities(database_t* database, cJSON* entities, int e)
{
    int rc = 0;
    for(cJSON* entity = entities->child; entity; entity = entity->next)
    {
        CHECK_ZERO(insert_entity(database, entity, e));
    }

cleanup:
    return rc;
}

static int setup_statements(database_t* database)
{
    int rc = 0;
    sqlite3* db = database->db;

    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  INSERT_USER,
                  strlen(INSERT_USER),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->create_stmts[0],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  INSERT_VISIT,
                  strlen(INSERT_VISIT),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->create_stmts[1],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  INSERT_LOCATION,
                  strlen(INSERT_LOCATION),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->create_stmts[2],
                  NULL));

    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  SELECT_USER,
                  strlen(SELECT_USER),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->read_stmts[0],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  SELECT_VISIT,
                  strlen(SELECT_VISIT),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->read_stmts[1],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  SELECT_LOCATION,
                  strlen(SELECT_LOCATION),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->read_stmts[2],
                  NULL));

    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  UPDATE_USER,
                  strlen(UPDATE_USER),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->write_stmts[0],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  UPDATE_VISIT,
                  strlen(UPDATE_VISIT),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->write_stmts[1],
                  NULL));
    CHECK_SQL(sqlite3_prepare_v3(
                  db,
                  UPDATE_LOCATION,
                  strlen(UPDATE_LOCATION),
                  SQLITE_PREPARE_PERSISTENT,
                  &database->write_stmts[2],
                  NULL));

cleanup:
    return rc;
}


static const char* OPTIONS = "options.txt";

int bootstrap(database_t* database, const char* filename)
{
    int rc = 0;

    sqlite3* db;
    VERIFY_ZERO(sqlite3_open(":memory:", &db));
    database->db = db;
    database->timestamp = time(NULL);

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));

    CHECK_SQL(sqlite3_exec(db, "PRAGMA synchronous = OFF",
                           NULL, NULL, NULL));
    CHECK_SQL(sqlite3_exec(db, "PRAGMA temp_store = MEMORY",
                           NULL, NULL, NULL));
    CHECK_SQL(sqlite3_exec(db, "PRAGMA journal_mode = OFF",
                           NULL, NULL, NULL));
    CHECK_SQL(sqlite3_exec(db, DDL, NULL, NULL, NULL));
    CHECK_SQL(setup_statements(database));
    CHECK_SQL(sqlite3_exec(db, "BEGIN", NULL, NULL, NULL));

#ifndef NDEBUG
    if(dump)
        printf("%s\nBEGIN;\n", DDL);
#endif  // NDEBUG

    if(!mz_zip_reader_init_file(&archive, filename, 0))
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
        if(verbose)
            printf("Processing %s...\n", stat.m_filename);

        char* data = mz_zip_reader_extract_to_heap(&archive, i, NULL, 0);
        if (!data)
        {
            // хромаем дальше
            fprintf(stderr, "Processing file '%s' in zip archive FAILED!\n",
                    stat.m_filename);
            continue;
        }

        if(strncmp(stat.m_filename, OPTIONS, strlen(OPTIONS)) == 0)
        {
            database->timestamp = atoi(data);
            continue;
        }

        cJSON* root = cJSON_Parse(data);
        if(root)
        {
            // XXX omitting some checks, assuming inputs are correct
            for(int i = 0; i < 3; i++)
            {
                cJSON* entities =
                    cJSON_GetObjectItemCaseSensitive(root, ENTITIES[i].name);
                if(entities)
                {
                    /* NOTE default SQLite's PRAGMA foreign_keys is OFF, so
                       we can safely insert visits without any particular order */
                    CHECK_ZERO(insert_entities(database, entities, i));
                    break;
                }
            }
        }
        else
        {
            // хромаем дальше
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
#ifndef NDEBUG
        if(dump)
            printf("COMMIT;\nANALYZE;\n");
#endif  // NDEBUG

        CHECK_SQL(sqlite3_exec(db, "COMMIT",
                               NULL, NULL, NULL));
        CHECK_SQL(sqlite3_exec(db, "PRAGMA foreign_keys = ON",
                               NULL, NULL, NULL));
        CHECK_SQL(sqlite3_exec(db, "ANALYZE",
                               NULL, NULL, NULL));
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
    char* body = alloca(length+1);
    memset(body, 0, length+1);
    CHECK_POSITIVE(evbuffer_remove(in_buf, body, length));

    CHECK_NONZERO(out_buf = evbuffer_new());
    database_t* database = (database_t*)arg;
    CHECK_SQL(sqlite3_exec(database->db, body, SQL_callback, out_buf, NULL));
    evhttp_send_reply(req, HTTP_OK, "OK", out_buf);
    rc = 0;

cleanup:
    if(out_buf)
        evbuffer_free(out_buf);
    return rc;
}
