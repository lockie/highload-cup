#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "response.h"
#include "entity.h"
#include "utils.h"
#include "methods.h"


const char* METHODS[2] = {"avg", "visits"};

static const char* AVG = "SELECT avg(visits.mark) FROM visits WHERE visits.location=%d";

static const char* FROM_DATE = " AND visits.visited_at>%d";

static const char* TO_DATE = " AND visits.visited_at<%d";

static const char* AVG_USER = "SELECT avg(visits.mark) FROM visits JOIN users "
    "ON visits.user=users.id WHERE visits.location=%d";

static const char* AVG_USER_GENDER = " AND users.gender=\"%c\"";

static const char* AVG_USER_FROM_AGE = " AND date(users.birth_date,'unixepoch')"
    "<date('%d','unixepoch','-%d years')";

static const char* AVG_USER_TO_AGE = " AND date(users.birth_date,'unixepoch')>"
    "date('%d','unixepoch','-%d years')";

int avg_callback(void* arg, int argc, char **argv, char **col)
{
    (void)argc;
    (void)col;
    double* avg = (double*)arg;
    if(LIKELY(argv[0]))
        *avg = atof(argv[0]);
    return 0;
}

int execute_avg(database_t* database, int id,
                const parameters_t* params, char* response)
{
    if(UNLIKELY(params->gender && params->gender != 'm' && params->gender != 'f'))
    {
        strcpy(response, "invalid gender");
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

    /* first, check the location exists */
    sqlite3_stmt* exists_stmt = database->exists_stmts[2];
    CHECK_SQL(sqlite3_reset(exists_stmt));
    CHECK_SQL(sqlite3_bind_int(exists_stmt, 1, id));
    CHECK_SQL(sqlite3_step(exists_stmt));
    if(UNLIKELY(!sqlite3_column_int(exists_stmt, 0)))
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
                        AVG_USER_FROM_AGE, database->timestamp, params->fromAge);
    if(params->toAge != INT_MAX)
        len += snprintf(&sql[len], sql_len-len,
                        AVG_USER_TO_AGE, database->timestamp, params->toAge);

    double average = 0;
    CHECK_SQL(sqlite3_exec(database->db, sql, avg_callback, &average, NULL));
    if(average == 0)
        strcpy(response, "{\"avg\": 0.0}");
    else
        snprintf(response, RESPONSE_BUFFER_SIZE,
                 "{\"avg\":%.5f}", average);

cleanup:
    return rc;
}

static const char* VISITS = "SELECT visits.mark,visits.visited_at,"
    "locations.place FROM visits JOIN locations ON visits.location=locations.id"
    " WHERE visits.user=%d";

static const char* VISITS_TO_DISTANCE = " AND locations.distance<%d";

static const char* VISITS_COUNTRY = " AND locations.country=\"%s\"";

static const char* VISITS_ORDER = " ORDER BY visits.visited_at";

static const char* VISIT_FORMAT = "{\"%s\":%s,\"%s\":%s,\"%s\":\"%s\"},";


int visits_callback(void* arg, int argc, char **argv, char **col)
{
    (void)argc;
    char** argptr = (char**)arg;
    char* response = argptr[0];
    char* buffer = argptr[1];
    argptr[1] = buffer + snprintf(buffer,
        RESPONSE_BUFFER_SIZE - (response - buffer),
        VISIT_FORMAT, col[0], argv[0], col[1], argv[1], col[2], argv[2]);
    return 0;
}

static const char* VISITS_RESULT_START = "{\"visits\":[ ";

int execute_visits(database_t* database, int id,
                   const parameters_t* params, char* response)
{
    int rc;
    const size_t sql_len = strlen(VISITS) + strlen(FROM_DATE) +
        strlen(TO_DATE) + strlen(VISITS_TO_DISTANCE) +
        strlen(VISITS_COUNTRY) + strlen(VISITS_ORDER) + 84;
    int len;
    char sql[sql_len];
    memset(sql, 0, sql_len);

    /* first, check the user exists */
    sqlite3_stmt* exists_stmt = database->exists_stmts[0];
    CHECK_SQL(sqlite3_reset(exists_stmt));
    CHECK_SQL(sqlite3_bind_int(exists_stmt, 1, id));
    CHECK_SQL(sqlite3_step(exists_stmt));
    if(UNLIKELY(!sqlite3_column_int(exists_stmt, 0)))
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
    if(params->country[0])
        len += snprintf(&sql[len], sql_len-len,
                        VISITS_COUNTRY, params->country);
    len += snprintf(&sql[len], sql_len-len,
                    "%s", VISITS_ORDER);

    strcpy(response, VISITS_RESULT_START);
    char* argptr[2] = {response, response + strlen(VISITS_RESULT_START)};
    CHECK_SQL(sqlite3_exec(database->db, sql, visits_callback, argptr, NULL));
    strcpy(argptr[1]-1, "]}");

cleanup:
    return rc;
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
