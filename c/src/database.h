#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <time.h>

#include <glib.h>

#include <event2/http.h>
#include <cJSON.h>

#include "utils.h"


typedef struct
{
    int id;
    char email[128];
    char first_name[64];
    char last_name[64];
    char gender[8];
    int birth_date;
} user_t;

typedef struct
{
    int id;
    char place[128];
    char country[64];
    char city[64];
    int distance;
} location_t;

typedef struct
{
    int id;
    int location;
    int user;
    int visited_at;
    int mark;
} visit_t;

typedef struct
{
    GPtrArray* entities[3]; // users, visits, locations. XXX all 1-based
    GPtrArray* visits_user_index;  // holds GSequence*s of visits for every user
    GPtrArray* visits_location_index;  // holds GPtrArray*s of visits for every location

    int timestamp;  // current timestamp from options.txt
    struct tm* timestamp_tm;  // same, in struct tm format
} database_t;

int bootstrap(database_t*, const char*);
int insert_entity(database_t*, cJSON*, int);
void set_phase(database_t* database, int phase);

void update_visits_user_index(database_t*, visit_t* visit, int add);
void update_visits_location_index(database_t*, visit_t* visit, int add);


#endif  // _DATABASE_H_
