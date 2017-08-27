#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <stdio.h>

#include <sqlite3.h>
#include <event2/http.h>
#include <cJSON.h>

#include "utils.h"


#define CHECK_SQL(x) {rc=(x); if(rc!=0&&rc!=SQLITE_DONE&&rc!=SQLITE_ROW){ \
            if(!(IS_SET(NDEBUG)))                                         \
                fprintf(stderr, "%s: SQL error %d: %s\n",                 \
                        #x, rc, sqlite3_errstr(rc));                      \
            goto cleanup;}}

typedef struct
{
    sqlite3* db;

    /* NOTE : order of statements is the same as in ENTITIES array */
    sqlite3_stmt* create_stmts[3];
    sqlite3_stmt* read_stmts[3];
    sqlite3_stmt* write_stmts[3];
} database_t;

int bootstrap(database_t*);
int process_SQL(struct evhttp_request*, void*);  // XXX debug
int insert_entity(database_t*, cJSON*, int);


#endif  // _DATABASE_H_
