#ifndef _REQUEST_H_
#define _REQUEST_H_

#include <event2/http.h>


void request_handler(struct evhttp_request* req, void* arg);

#endif  // _REQUEST_H_
