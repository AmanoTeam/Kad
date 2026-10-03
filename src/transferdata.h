#if !defined(TRANSFERDATA_H)
#define TRANSFERDATA_H

#include "ssl.h"
#include "http.h"
#include "buffer.h"

struct transferdata {
	ssl_context_t* context;
	http_request_t* request;
	int fd;
	int is_secure;
	size_t remaining;
	buffer_t buffer;
};

typedef struct transferdata transferdata_t;

#endif
