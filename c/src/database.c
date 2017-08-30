#include <miniz_zip.h>

#include "entity.h"
#include "database.h"


static gint visits_comparator(gconstpointer a,  gconstpointer b, gpointer data)
{
    (void)data;
    int a_visited = ((visit_t*)a)->visited_at;
    int b_visited = ((visit_t*)b)->visited_at;
    if(a_visited > b_visited)
        return 1;
    if(a_visited < b_visited)
        return -1;
    return 0;  // equal
}

void update_visits_user_index(database_t* database, visit_t* visit, int add)
{
    if(add)
    {
        int user_id = visit->user;
        if(UNLIKELY(database->visits_user_index->len < (guint)user_id))
            g_ptr_array_set_size(database->visits_user_index, user_id * 2);
        GSequence* user_visits = g_ptr_array_index(
            database->visits_user_index, user_id);
        if(UNLIKELY(!user_visits))
        {
            user_visits = g_sequence_new(NULL);
            g_ptr_array_index(database->visits_user_index, user_id) =
                user_visits;
        }
        g_sequence_insert_sorted(user_visits, visit, visits_comparator, NULL);
    }
    else
    {
        GSequence* user_visits = g_ptr_array_index(
            database->visits_user_index, visit->user);
        g_sequence_remove(
            g_sequence_lookup(
                user_visits, visit, visits_comparator, NULL));
    }
}

void update_visits_location_index(database_t* database, visit_t* visit, int add)
{
    if(add)
    {
        int location_id = visit->location;
        if(UNLIKELY(database->visits_location_index->len < (guint)location_id))
            g_ptr_array_set_size(database->visits_location_index, location_id*2);
        GPtrArray* location_visits = g_ptr_array_index(
            database->visits_location_index, location_id);
        if(UNLIKELY(!location_visits))
        {
            location_visits = g_ptr_array_new();
            g_ptr_array_index(database->visits_location_index, location_id) =
                location_visits;
        }
        g_ptr_array_add(location_visits, visit);
    }
    else
    {
        GPtrArray* location_visits = g_ptr_array_index(
            database->visits_location_index, visit->location);
        g_ptr_array_remove_fast(location_visits, visit);
    }
}

int insert_entity(database_t* database, cJSON* json, int e)
{
    if(LIKELY(phase_hack))
        set_phase(database, 2);

    const entity_t* entity = &ENTITIES[e];

    cJSON* id_json = cJSON_GetObjectItemCaseSensitive(json, "id");
    if(UNLIKELY(!id_json || !cJSON_IsNumber(id_json)))
        return PROCESS_RESULT_BAD_REQUEST;
    int id = id_json->valueint;

    void* ptr = malloc(entity->size);
    if(!ptr)
        return PROCESS_RESULT_ERROR;

    *(int*)ptr = id;
    for(int i = 0; i < 5; i++)
    {
        if(UNLIKELY(entity->column_types[i] != COLUMN_TYPE_NONE))
        {
            cJSON* arg = cJSON_GetObjectItemCaseSensitive(
                json, entity->column_names[i]);
            if(UNLIKELY(!arg || cJSON_IsNull(arg)))
                return PROCESS_RESULT_BAD_REQUEST;

            if(entity->column_types[i] == COLUMN_TYPE_INT)
            {
                *(int*)((char*)ptr + entity->column_offsets[i]) = arg->valueint;
            }
            else
            {
                strncpy((char*)ptr + entity->column_offsets[i],
                        arg->valuestring,
                        entity->column_sizes[i]);
            }
        }
    }
    if(database->entities[e]->len < (guint)id)
        // poor man's preallocation
        g_ptr_array_set_size(database->entities[e], id*2);
    g_ptr_array_index(database->entities[e], id) = ptr;

    if(e == 1)  // visit. XXX relying on fact that visits are loaded last
    {
        update_visits_user_index(database, ptr, 1);
        update_visits_location_index(database, ptr, 1);
    }

    return PROCESS_RESULT_OK;
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

static const char* OPTIONS = "options.txt";

int bootstrap(database_t* database, const char* filename)
{
    int rc = 0;

    database->timestamp = time(NULL);
    database->phase = 0;

    database->entities[0] = g_ptr_array_sized_new(16384);
    g_ptr_array_set_size(database->entities[0], 16384);
    database->entities[1] = g_ptr_array_sized_new(131072);
    g_ptr_array_set_size(database->entities[1], 131072);
    database->entities[2] = g_ptr_array_sized_new(16384);
    g_ptr_array_set_size(database->entities[2], 16384);
    database->visits_user_index = g_ptr_array_sized_new(16384);
    g_ptr_array_set_size(database->visits_user_index, 16384);
    database->visits_location_index = g_ptr_array_sized_new(16384);
    g_ptr_array_set_size(database->visits_location_index, 16384);

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, filename, 0))
        return mz_zip_get_last_error(&archive);

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
        if(phase_hack)
            database->phase = 1;
        database->timestamp_tm = gmtime((time_t*)&database->timestamp);
        database->timestamp_tm->tm_isdst = 0;
    }
    return rc;
}

void set_phase(database_t* database, int phase)
{
    if(phase == 2)
    {
        if(UNLIKELY(database->phase == 1))
        {
            database->phase = 2;
            // XXX currently no-op
            printf("Detected phase 2 start\n");
        }
    }
    if(phase == 3)
    {
        if(UNLIKELY(database->phase == 2))
        {
            database->phase = 3;
            // XXX currently no-op
            printf("Detected phase 3 start\n");
        }
    }
}
