#ifndef _DATABASE_H_
#define _DATABASE_H_

#include <stdio.h>

#include <sqlite3.h>
#include <event2/http.h>

#include "utils.h"


#define CHECK_SQL(x) {rc=(x); if(rc!=0&&rc!=SQLITE_DONE){   \
            if(!(IS_SET(NDEBUG)))                           \
                fprintf(stderr, "%s: SQL error %d: %s\n",   \
                        #x, rc, sqlite3_errstr(rc));        \
            goto cleanup;}}

int bootstrap(sqlite3* db);
int process_SQL(struct evhttp_request* req, void* arg);  // XXX debug


#endif  // _DATABASE_H_
