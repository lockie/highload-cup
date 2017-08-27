#ifndef _ENTITY_H_
#define _ENTITY_H_

#include "database.h"


#define PROCESS_RESULT_OK 0
#define PROCESS_RESULT_BAD_REQUEST 1
#define PROCESS_RESULT_NOT_FOUND 2
#define PROCESS_RESULT_ERROR 3

#define COLUMN_TYPE_NONE -1
#define COLUMN_TYPE_INT 0
#define COLUMN_TYPE_STR 1

typedef struct
{
    const char* name;
    int column_types[5];
    const char* column_names[5];
    const char* format;
    size_t extrasize;
} entity_t;

extern const entity_t ENTITIES[3];

typedef struct
{
    int fromDate;
    int toDate;

    const char* country;
    int toDistance;

    int fromAge;
    int toAge;
    char gender;
} parameters_t;

/* NOTE entity is index in ENTITIES array */
int process_entity(database_t* database,
                   int entity, int id, int write,
                   const char* body, char** response);

#endif  // _ENTITY_H_
