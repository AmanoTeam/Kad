#if !defined(TRANSFERDATA_H)
#define TRANSFERDATA_H

#include <curl/curl.h>

#include "ssl.h"
#include "http.h"
#include "buffer.h"

#if defined(KAD_CAPTURE)
	#include "capture.h"
#endif

struct transferdata {
	ssl_context_t context;
	http_request_t request;
	struct curl_slist* headers;
#if defined(KAD_CAPTURE)
	CURL* curl;
	capture_t capture;
#endif
	int fd;
	int is_secure;
	size_t remaining;
	buffer_t buffer;
};

typedef struct transferdata transferdata_t;

#endif
