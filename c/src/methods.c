#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "response.h"
#include "entity.h"
#include "utils.h"
#include "methods.h"


const char* METHODS[2] = {"avg", "visits"};

static inline int datediff(const database_t* database, int years)
{
    struct tm timestruct;
    memcpy(&timestruct, database->timestamp_tm, sizeof(struct tm));
    timestruct.tm_year -= years;
    return mktime(&timestruct);
}

int execute_avg(database_t* database, int id,
                const parameters_t* params, char* response)
{
    if(UNLIKELY(params->gender &&
                params->gender != 'm' && params->gender != 'f'))
    {
        strcpy(response, "invalid gender");
        return PROCESS_RESULT_BAD_REQUEST;
    }

    /* first, check the location exists */
    GPtrArray* arr = database->entities[2];
    if(UNLIKELY((guint)id >= arr->len))
        return PROCESS_RESULT_NOT_FOUND;
    void* ptr = g_ptr_array_index(arr, id);
    if(UNLIKELY(ptr == NULL))
        return PROCESS_RESULT_NOT_FOUND;

    GPtrArray* visits = NULL;
    if(UNLIKELY((guint)id >= database->visits_location_index->len ||
                !(visits = g_ptr_array_index(database->visits_location_index,
                                             id)) || visits->len == 0))
    {
        strcpy(response, "{\"avg\": 0.0}");
        return PROCESS_RESULT_OK;
    }

    double sum = 0;
    size_t n = 0;
    for(guint i = 0; i < visits->len; i++)
    {
        visit_t* visit = g_ptr_array_index(visits, i);

        if(params->fromDate != INT_MAX && visit->visited_at <= params->fromDate)
            continue;
        if(params->toDate != INT_MAX && visit->visited_at >= params->toDate)
            continue;

        user_t* user = NULL;
        if((guint)visit->user < database->entities[0]->len)
            user = g_ptr_array_index(database->entities[0], visit->user);
        // XXX user = NULL means invalid data
        if(UNLIKELY(!user))
            continue;

        if(params->gender != 0 && user->gender[0] != params->gender)
            continue;
        if(params->fromAge != INT_MAX &&
           user->birth_date >= datediff(database, params->fromAge))
            continue;
        if(params->toAge != INT_MAX &&
           user->birth_date <= datediff(database, params->toAge))
            continue;

        sum += visit->mark; n++;
    }

    if(n == 0)
        strcpy(response, "{\"avg\": 0.0}");
    else
        snprintf(response, RESPONSE_BUFFER_SIZE,
                 "{\"avg\":%.5f}", sum / n + 1e-10);
    return PROCESS_RESULT_OK;
}

static const char* VISIT_FORMAT =
    "{\"mark\":%d,\"visited_at\":%d,\"place\":\"%s\"},";

static const char* VISITS_RESULT_START = "{\"visits\":[ ";

int execute_visits(database_t* database, int id,
                   const parameters_t* params, char* response)
{
    /* first, check the user exists */
    GPtrArray* arr = database->entities[0];
    if(UNLIKELY((guint)id >= arr->len))
        return PROCESS_RESULT_NOT_FOUND;
    void* ptr = g_ptr_array_index(arr, id);
    if(UNLIKELY(ptr == NULL))
        return PROCESS_RESULT_NOT_FOUND;

    GSequence* visits = NULL;
    if(UNLIKELY((guint)id >= database->visits_user_index->len ||
                !(visits = g_ptr_array_index(database->visits_user_index, id)) ||
                g_sequence_is_empty(visits)))
    {
        strcpy(response, "{\"visits\":[]}");
        return PROCESS_RESULT_OK;
    }

    strcpy(response, VISITS_RESULT_START);
    char* buffer = response + strlen(VISITS_RESULT_START);
    for(GSequenceIter* it = g_sequence_get_begin_iter(visits);
        !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
    {
        visit_t* visit = g_sequence_get(it);

        if(params->fromDate != INT_MAX && visit->visited_at <= params->fromDate)
            continue;
        if(params->toDate != INT_MAX && visit->visited_at >= params->toDate)
            continue;

        location_t* location = NULL;
        if((guint)visit->location < database->entities[2]->len)
            location = g_ptr_array_index(database->entities[2], visit->location);
        // XXX location = NULL means invalid data
        if(!location)
            continue;
        if(params->toDistance != INT_MAX &&
           location->distance >= params->toDistance)
            continue;
        if(params->country[0] && strncmp(location->country, params->country,
                                         sizeof(params->country)) != 0)
            continue;

        buffer += snprintf(buffer,
                           RESPONSE_BUFFER_SIZE - (response - buffer),
                           VISIT_FORMAT,
                           visit->mark,
                           visit->visited_at,
                           location->place);
    }
    strcpy(buffer-1, "]}");
    return PROCESS_RESULT_OK;
}

int execute_method(database_t* database, int entity, int id, int method,
                   const parameters_t* params, char* response)
{
    switch(method)
    {
    case METHOD_AVG:
        if(UNLIKELY(entity != 2))
            return PROCESS_RESULT_NOT_FOUND;
        return execute_avg(database, id, params, response);
        break;

    case METHOD_VISITS:
        if(UNLIKELY(entity != 0))
            return PROCESS_RESULT_NOT_FOUND;
        return execute_visits(database, id, params, response);
        break;

    default:
        return PROCESS_RESULT_NOT_FOUND;
    }
}
