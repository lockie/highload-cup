#ifndef _REQUEST_H_
#define _REQUEST_H_

#include <event2/http.h>


#define METHOD_DEFAULT -1
#define METHOD_AVG  0
#define METHOD_VISITS 1

void request_handler(struct evhttp_request* req, void* arg);

#endif  // _REQUEST_H_
