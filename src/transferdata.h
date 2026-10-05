#if !defined(TRANSFERDATA_H)
#define TRANSFERDATA_H

#include <curl/curl.h>

#include "ssl.h"
#include "http.h"
#include "buffer.h"

struct transferdata {
	ssl_context_t context;
	http_request_t request;
	struct curl_slist* headers;
	int fd;
	int is_secure;
	size_t remaining;
	buffer_t buffer;
};

typedef struct transferdata transferdata_t;

#endif
