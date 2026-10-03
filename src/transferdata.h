#if !defined(TRANSFERDATA_H)
#define TRANSFERDATA_H

#include "ssl.h"
#include "http.h"
#include "buffer.h"

struct transferdata {
	ssl_context_t* context;
	http_request_t* request;
	http_response_t* response;
	int fd;
	int is_secure;
	int eof;
	buffer_t buffer;
};

typedef struct transferdata transferdata_t;

#endif
