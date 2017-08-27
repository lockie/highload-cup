#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "entity.h"
#include "utils.h"
#include "methods.h"


const char* METHODS[2] = {"avg", "visits"};

static const char* AVG = R"(
SELECT avg(visits.mark) FROM visits WHERE visits.location=%d
)";

static const char* FROM_DATE = " AND visits.visited_at>%d";

static const char* TO_DATE = " AND visits.visited_at<%d";

static const char* AVG_USER = R"(
SELECT avg(visits.mark) FROM visits JOIN users ON visits.user=users.id
WHERE visits.location=%d
)";

static const char* AVG_USER_GENDER = " AND users.gender=\"%c\"";

static const char* AVG_USER_FROM_AGE = R"(
AND date(users.birth_date, 'unixepoch')<date('now', '-%d years')
)";

static const char* AVG_USER_TO_AGE = R"(
AND date(users.birth_date, 'unixepoch')>date('now', '-%d years')
)";

int avg_callback(void* arg, int argc, char **argv, char **col)
{
    (void)argc;
    (void)col;
    double* avg = (double*)arg;
    if(argv[0])
        *avg = atof(argv[0]);
    return 0;
}

int execute_avg(database_t* database, int id,
                const parameters_t* params, char** response)
{
    if(params->gender && params->gender != 'm' && params->gender != 'f')
    {
        *response = strdup("invalid gender");
        return PROCESS_RESULT_BAD_REQUEST;
    }

    int with_user = 0;
    if(params->fromAge != INT_MAX || params->toAge != INT_MAX ||
       params->gender != 0)
        with_user = 1;

    int rc;

    const size_t sql_len = strlen(AVG_USER) + strlen(FROM_DATE) +
        strlen(TO_DATE) + strlen(AVG_USER_GENDER) +
        strlen(AVG_USER_FROM_AGE) + strlen(AVG_USER_TO_AGE) + 54;
    int len;
    char sql[sql_len];
    memset(sql, 0, sql_len);

    /* first, check the location exists by SELECTing it */
    /* XXX room for optimization here: its possible to use EXISTS query */
    sqlite3_stmt* read_stmt = database->read_stmts[2];
    CHECK_SQL(sqlite3_reset(read_stmt));
    CHECK_SQL(sqlite3_bind_int(read_stmt, 1, id));
    rc = sqlite3_step(read_stmt);
    if(rc == SQLITE_DONE)
        return PROCESS_RESULT_NOT_FOUND;

    /* XXX room for optimization here: prepare statement beforehand. this could
       save couple of cents. */
    len = snprintf(sql, sql_len, with_user ? AVG_USER : AVG, id);
    if(params->fromDate != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        FROM_DATE, params->fromDate);
    if(params->toDate != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        TO_DATE, params->toDate);
    if(params->gender != 0)
        len += snprintf(&sql[len], sql_len-len,
                        AVG_USER_GENDER, params->gender);
    if(params->fromAge != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        AVG_USER_FROM_AGE, params->fromAge);
    if(params->toAge != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        AVG_USER_TO_AGE, params->toAge);

    double average = 0;
    CHECK_SQL(sqlite3_exec(database->db, sql, avg_callback, &average, NULL));
    if(average == 0)
    {
        *response = strdup("{\"avg\": 0.0}");
    }
    else
    {
        *response = malloc(16);
        snprintf(*response, 16, "{\"avg\":%.5f}", average);
    }

cleanup:
    return rc;
}

static const char* VISITS = R"(
SELECT visits.mark,visits.visited_at,locations.place FROM visits JOIN locations
ON visits.location=locations.id WHERE visits.user=%d
)";


static const char* VISITS_TO_DISTANCE = " AND locations.distance<%d";

static const char* VISITS_COUNTRY = " AND locations.country=\"%s\"";

static const char* VISITS_ORDER = " ORDER BY visits.visited_at";

static const char* VISIT_FORMAT = "{\"%s\":%s,\"%s\":%s,\"%s\":\"%s\"},";

int visits_callback(void* arg, int argc, char **argv, char **col)
{
    (void)argc;
    char** response = (char**)arg;

    /* TODO : probably use cJSON here to avoid bunch of mallocs */
    int n = snprintf(NULL, 0, VISIT_FORMAT,
                     col[0], argv[0], col[1], argv[1], col[2], argv[2]);

    size_t len = strlen(*response);
    *response = realloc(*response, len+n+1);
    snprintf(&(*response)[len], n+1, VISIT_FORMAT,
             col[0], argv[0], col[1], argv[1], col[2], argv[2]);
    return 0;
}

int execute_visits(database_t* database, int id,
                   const parameters_t* params, char** response)
{
    int rc;
    const size_t sql_len = strlen(VISITS) + strlen(FROM_DATE) +
        strlen(TO_DATE) + strlen(VISITS_TO_DISTANCE) +
        strlen(VISITS_COUNTRY) + strlen(VISITS_ORDER) + 84;
    int len;
    char sql[sql_len];
    memset(sql, 0, sql_len);

    /* first, check the user exists by SELECTing it */
    /* XXX room for optimization here: its possible to use EXISTS query */
    sqlite3_stmt* read_stmt = database->read_stmts[0];
    CHECK_SQL(sqlite3_reset(read_stmt));
    CHECK_SQL(sqlite3_bind_int(read_stmt, 1, id));
    rc = sqlite3_step(read_stmt);
    if(rc == SQLITE_DONE)
        return PROCESS_RESULT_NOT_FOUND;

    /* XXX room for optimization here: prepare statement beforehand. this could
       save couple of cents. */
    len = snprintf(sql, sql_len, VISITS, id);
    if(params->fromDate != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        FROM_DATE, params->fromDate);
    if(params->toDate != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        TO_DATE, params->toDate);
    if(params->toDistance != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        VISITS_TO_DISTANCE, params->toDistance);
    if(params->country)
        len += snprintf(&sql[len], sql_len-len,
                        VISITS_COUNTRY, params->country);
    len += snprintf(&sql[len], sql_len-len,
                    "%s", VISITS_ORDER);

    *response = strdup("{\"visits\":[ ");


    CHECK_SQL(sqlite3_exec(database->db, sql, visits_callback, response, NULL));

    size_t rlen = strlen(*response);
    *response = realloc(*response, rlen + 2);
    strcpy(&(*response)[rlen-1], "]}");

cleanup:
    return rc;
}

int execute_method(database_t* database, int entity, int id, int method,
                   const parameters_t* params, char** response)
{
    switch(method)
    {
    case METHOD_AVG:
        if(entity != 2)
            // wut
            return PROCESS_RESULT_NOT_FOUND;
        return execute_avg(database, id, params, response);
        break;

    case METHOD_VISITS:
        if(entity != 0)
            // wut
            return PROCESS_RESULT_NOT_FOUND;
        return execute_visits(database, id, params, response);
        break;

    default:
        return PROCESS_RESULT_NOT_FOUND;
    }
}
