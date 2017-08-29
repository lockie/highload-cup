#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "response.h"
#include "utils.h"
#include "entity.h"


const entity_t ENTITIES[3] = {
    {"users", {COLUMN_TYPE_STR, COLUMN_TYPE_STR, COLUMN_TYPE_STR,
               COLUMN_TYPE_STR, COLUMN_TYPE_INT},
     {"email", "first_name", "last_name", "gender", "birth_date"},
     "{\"id\":%d,\"email\":\"%s\",\"first_name\":\"%s\",\"last_name\":\"%s\","
     "\"gender\":\"%s\",\"birth_date\":%d}",
    },
    {"visits", {COLUMN_TYPE_INT, COLUMN_TYPE_INT, COLUMN_TYPE_INT,
                COLUMN_TYPE_INT, COLUMN_TYPE_NONE},
     {"location", "user", "visited_at", "mark", NULL},
     "{\"id\":%d,\"location\":%d,\"user\":%d,\"visited_at\":%d,\"mark\":%d}",
    },
    {"locations", {COLUMN_TYPE_STR, COLUMN_TYPE_STR, COLUMN_TYPE_STR,
                   COLUMN_TYPE_INT, COLUMN_TYPE_NONE},
     {"place", "country", "city", "distance", NULL},
     "{\"id\":%d,\"place\":\"%s\",\"country\":\"%s\",\"city\":\"%s\","
     "\"distance\":%d}",
    }
};

static inline const void* construct_arg(sqlite3_stmt* stmt,
                                        const entity_t* entity, int i)
{
    if(UNLIKELY(entity->column_types[i] == COLUMN_TYPE_NONE))
        return NULL;
    return entity->column_types[i] == COLUMN_TYPE_INT ?
        (const void*)(intptr_t)sqlite3_column_int(stmt, i+1) :
        (const void*)sqlite3_column_text(stmt, i+1);
}

static int read_entity(database_t* database, int e, int id, char* response)
{
    if(LIKELY(phase_hack))
        set_phase(database, 3);

    int rc;
    const entity_t* entity = &ENTITIES[e];
    sqlite3_stmt* stmt = database->read_stmts[e];
    CHECK_SQL(sqlite3_reset(stmt));
    CHECK_SQL(sqlite3_bind_int(stmt, 1, id));
    CHECK_SQL(sqlite3_step(stmt));
    if(UNLIKELY(rc == SQLITE_DONE))
        return -1;
    rc = 0;
    snprintf(response, RESPONSE_BUFFER_SIZE,
                     entity->format,
                     sqlite3_column_int(stmt, 0),  /* id */
                     construct_arg(stmt, entity, 0),
                     construct_arg(stmt, entity, 1),
                     construct_arg(stmt, entity, 2),
                     construct_arg(stmt, entity, 3),
                     construct_arg(stmt, entity, 4));

cleanup:
    return rc;
}

static inline int bind_val(sqlite3_stmt* stmt, sqlite3_stmt* read_stmt,
                           const entity_t* entity, cJSON* json, int i)
{
    if(UNLIKELY(entity->column_types[i] == COLUMN_TYPE_NONE))
        return 0;

    int rc;
    const void* value;
    cJSON* arg = cJSON_GetObjectItemCaseSensitive(json, entity->column_names[i]);
    if(!arg)
    {
        if(entity->column_types[i] == COLUMN_TYPE_INT)
            value = (void*)(intptr_t)sqlite3_column_int(read_stmt, i+1);
        else
            value = sqlite3_column_text(read_stmt, i+1);
    }
    else
    {
        if(UNLIKELY(cJSON_IsNull(arg)))
            return PROCESS_RESULT_BAD_REQUEST;
        if(entity->column_types[i] == COLUMN_TYPE_INT)
            value = (void*)(intptr_t)arg->valueint;
        else
            value = arg->valuestring;
    }

    if(entity->column_types[i] == COLUMN_TYPE_INT)
    {
        CHECK_SQL(sqlite3_bind_int(stmt, i+2, (intptr_t)value));
    }
    else
    {
        CHECK_SQL(sqlite3_bind_text(stmt, i+2, value, -1, SQLITE_STATIC));
    }

cleanup:
    return rc;
}

static int update_entity(database_t* database, cJSON* json, int e, int id)
{
    int rc;
    const entity_t* entity = &ENTITIES[e];

    if(LIKELY(phase_hack))
        set_phase(database, 2);

    /* first, check the entity even exists by SELECTing it */
    sqlite3_stmt* read_stmt = database->read_stmts[e];
    CHECK_SQL(sqlite3_reset(read_stmt));
    CHECK_SQL(sqlite3_bind_int(read_stmt, 1, id));
    rc = sqlite3_step(read_stmt);
    if(UNLIKELY(rc != SQLITE_ROW))
    {
        // assuming entity does not exist
        return PROCESS_RESULT_NOT_FOUND;
    }

    /* XXX room for optimization: its possible to update only the fields passed
       in JSON, but this means we'll have to either (1) prepare statements for
       all possible field combinations, which is quite a bunch, or even (2)
       refuse from preparing statements in advance. Both are bad. */
    sqlite3_stmt* stmt = database->write_stmts[e];
    CHECK_SQL(sqlite3_reset(stmt));
    CHECK_SQL(sqlite3_bind_int(stmt, 1, id)); // id
    for(int i = 0; i < 5; i++)
    {
        CHECK_ZERO(bind_val(stmt, read_stmt, entity, json, i));
    }
    CHECK_SQL(sqlite3_step(stmt));
    if(LIKELY(rc == SQLITE_DONE))
        rc = 0;

cleanup:
    return rc;
}

static int create_entity(database_t* database, cJSON* json, int e)
{
    int rc;

    cJSON* json_id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if(UNLIKELY(!json_id || !cJSON_IsNumber(json_id)))
    {
        return PROCESS_RESULT_BAD_REQUEST;
    }
    int id = json_id->valueint;
    /* first, check the entity already exists */
    sqlite3_stmt* exists_stmt = database->exists_stmts[e];
    CHECK_SQL(sqlite3_reset(exists_stmt));
    CHECK_SQL(sqlite3_bind_int(exists_stmt, 1, id));
    CHECK_SQL(sqlite3_step(exists_stmt));
    if(UNLIKELY(sqlite3_column_int(exists_stmt, 0)))
        return PROCESS_RESULT_BAD_REQUEST;

    CHECK_ZERO(insert_entity(database, json, e));

cleanup:
    return rc;
}

int process_entity(database_t* database,
                   int entity, int id, int write,
                   const char* body, char* response)
{
    if(write)
    {
        cJSON* root = cJSON_Parse(body);
        if(UNLIKELY(!root))
        {
            strcpy(response, "failed to parse JSON");
            return PROCESS_RESULT_BAD_REQUEST;
        }

        if(id == -1)  // new
        {
            int rc = create_entity(database, root, entity);
            cJSON_Delete(root);
            strcpy(response, "{}");
            return rc;
        }

        int rc = update_entity(database, root, entity, id);
        cJSON_Delete(root);
        strcpy(response, "{}");
        return rc;
    }

    return read_entity(database, entity, id, response) == 0 ?
        PROCESS_RESULT_OK :
        PROCESS_RESULT_NOT_FOUND;
}
