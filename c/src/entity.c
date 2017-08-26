#include <string.h>

#include <cJSON.h>

#include "entity.h"


int process_entity(const char* entity, int id, int method, int write,
                   const parameters_t* parameters,
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
