#ifndef _METHODS_H_
#define _METHODS_H_

#include "database.h"


typedef struct
{
    int fromDate;
    int toDate;

    char country[64];
    int toDistance;

    int fromAge;
    int toAge;
    char gender;
} parameters_t;

#define METHOD_DEFAULT -1
#define METHOD_AVG  0
#define METHOD_VISITS 1

extern const char* METHODS[2];

int execute_method(database_t* database, int entity, int id, int method,
                   const parameters_t* params, char* response);


#endif  // _METHOD_H_
