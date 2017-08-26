#ifndef _ENTITY_H_
#define _ENTITY_H_

#define METHOD_DEFAULT -1
#define METHOD_AVG  0
#define METHOD_VISITS 1

#define PROCESS_RESULT_OK 0
#define PROCESS_RESULT_BAD_REQUEST 1
#define PROCESS_RESULT_NOT_FOUND 2
#define PROCESS_RESULT_ERROR 3

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

int process_entity(const char* entity, int id, int method, int write,
                   const parameters_t* parameters,
                   const char* body, char** response);

#endif  // _ENTITY_H_
