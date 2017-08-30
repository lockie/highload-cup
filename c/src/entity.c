#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "response.h"
#include "database.h"
#include "utils.h"
#include "entity.h"


/* Wild poor man's OOP here */
#define membersize(type, member) sizeof(((type *)0)->member)

const entity_t ENTITIES[3] = {
    {"users", 5, {COLUMN_TYPE_STR, COLUMN_TYPE_STR, COLUMN_TYPE_STR,
               COLUMN_TYPE_STR, COLUMN_TYPE_INT},
     {"email", "first_name", "last_name", "gender", "birth_date"},
     {offsetof(user_t, email), offsetof(user_t, first_name),
      offsetof(user_t, last_name), offsetof(user_t, gender),
      offsetof(user_t, birth_date)},
     {membersize(user_t, email), membersize(user_t, first_name),
      membersize(user_t, last_name), membersize(user_t, gender),
      membersize(user_t, birth_date)},
     "{\"id\":%d,\"email\":\"%s\",\"first_name\":\"%s\",\"last_name\":\"%s\","
     "\"gender\":\"%s\",\"birth_date\":%d}",
     sizeof(user_t)
    },
    {"visits", 6, {COLUMN_TYPE_INT, COLUMN_TYPE_INT, COLUMN_TYPE_INT,
                COLUMN_TYPE_INT, COLUMN_TYPE_NONE},
     {"location", "user", "visited_at", "mark", NULL},
     {offsetof(visit_t, location), offsetof(visit_t, user),
      offsetof(visit_t, visited_at), offsetof(visit_t, mark), -1},
     {membersize(visit_t, location), membersize(visit_t, user),
      membersize(visit_t, visited_at), membersize(visit_t, mark), -1},
     "{\"id\":%d,\"location\":%d,\"user\":%d,\"visited_at\":%d,\"mark\":%d}",
     sizeof(visit_t)
    },
    {"locations", 9, {COLUMN_TYPE_STR, COLUMN_TYPE_STR, COLUMN_TYPE_STR,
                   COLUMN_TYPE_INT, COLUMN_TYPE_NONE},
     {"place", "country", "city", "distance", NULL},
     {offsetof(location_t, place), offsetof(location_t, country),
      offsetof(location_t, city), offsetof(location_t, distance), -1},
     {membersize(location_t, place), membersize(location_t, country),
      membersize(location_t, city), membersize(location_t, distance), -1},
     "{\"id\":%d,\"place\":\"%s\",\"country\":\"%s\",\"city\":\"%s\","
     "\"distance\":%d}",
     sizeof(location_t)
    }
};

static inline const void* construct_arg(void* ptr, const entity_t* entity, int i)
{
    if(UNLIKELY(entity->column_types[i] == COLUMN_TYPE_NONE))
        return NULL;
    char* arg = (char*)ptr + entity->column_offsets[i];
    const void* res = entity->column_types[i] == COLUMN_TYPE_INT ?
        (const void*)(intptr_t)(*(int*)arg) : arg;
    return res;
}

static int read_entity(database_t* database, int e, int id, char* response)
{
    const entity_t* entity = &ENTITIES[e];
    GPtrArray* arr = database->entities[e];

    if(UNLIKELY((guint)id >= arr->len))
        return PROCESS_RESULT_NOT_FOUND;
    void* ptr = g_ptr_array_index(arr, id);
    if(UNLIKELY(ptr == NULL))
        return PROCESS_RESULT_NOT_FOUND;

    snprintf(response, RESPONSE_BUFFER_SIZE,
             entity->format, id,
             construct_arg(ptr, entity, 0),
             construct_arg(ptr, entity, 1),
             construct_arg(ptr, entity, 2),
             construct_arg(ptr, entity, 3),
             construct_arg(ptr, entity, 4));
    return 0;
}

static int update_entity(database_t* database, cJSON* json, int e, int id)
{
    const entity_t* entity = &ENTITIES[e];
    GPtrArray* arr = database->entities[e];

    /* first, check the entity even exists */
    if(UNLIKELY((guint)id >= arr->len))
        return PROCESS_RESULT_NOT_FOUND;
    void* ptr = g_ptr_array_index(arr, id);
    if(UNLIKELY(ptr == NULL))
        return PROCESS_RESULT_NOT_FOUND;

    /* validate body first */
    cJSON* args[5];
    int none_set = 1;
    for(int i = 0; i < 5; i++)
    {
        if(UNLIKELY(entity->column_types[i] == COLUMN_TYPE_NONE))
            break;
        if(!(args[i] = cJSON_GetObjectItemCaseSensitive(
                 json, entity->column_names[i])))
            continue;
        none_set = 0;
        if(UNLIKELY(cJSON_IsNull(args[i])))
            return PROCESS_RESULT_BAD_REQUEST;
    }
    if(none_set)
        return PROCESS_RESULT_BAD_REQUEST;

    /* temporarily remove from indexes */
    if(e == 1 && args[0])
        update_visits_location_index(database, ptr, 0);
    if(e == 1 && (args[1] || args[2]))
        update_visits_user_index(database, ptr, 0);

    /* now set the fields */
    for(int i = 0; i < 5; i++)
    {
        if(UNLIKELY(entity->column_types[i] == COLUMN_TYPE_NONE))
            break;
        if(!args[i])
            continue;
        if(entity->column_types[i] == COLUMN_TYPE_INT)
            *(int*)((char*)ptr + entity->column_offsets[i]) = args[i]->valueint;
        else
            strncpy((char*)ptr + entity->column_offsets[i],
                    args[i]->valuestring, entity->column_sizes[i]);
    }

    /* update indexes */
    if(e == 1 && args[0])
        update_visits_location_index(database, ptr, 1);
    if(e == 1 && (args[1] || args[2]))
        update_visits_user_index(database, ptr, 1);

    return PROCESS_RESULT_OK;
}

static int create_entity(database_t* database, cJSON* json, int e)
{
    cJSON* json_id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if(UNLIKELY(!json_id || !cJSON_IsNumber(json_id)))
    {
        return PROCESS_RESULT_BAD_REQUEST;
    }
    int id = json_id->valueint;

    GPtrArray* arr = database->entities[e];

    /* first, check the entity already exists */
    void* ptr = NULL;
    if(UNLIKELY((guint)id < arr->len))
        ptr = g_ptr_array_index(arr, id);
    if(UNLIKELY(ptr != NULL))
        return PROCESS_RESULT_BAD_REQUEST;

    return insert_entity(database, json, e);
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

        int rc;
        if(id == -1)  // new
            rc = create_entity(database, root, entity);
        else
            rc = update_entity(database, root, entity, id);
        cJSON_Delete(root);
        strcpy(response, "{}");
        return rc;
    }
    return read_entity(database, entity, id, response) == 0 ?
        PROCESS_RESULT_OK :
        PROCESS_RESULT_NOT_FOUND;
}
