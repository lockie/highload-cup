#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include <event2/http.h>


void handle_bad_request(struct evhttp_request*, const char*);
void handle_not_found(struct evhttp_request*);

#endif  // _RESPONSE_H_
