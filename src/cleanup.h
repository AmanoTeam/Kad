#if !defined(CLEANUP_H)
#define CLEANUP_H

#include <curl/curl.h>

#include "http.h"
#include "ssl.h"

void __close(int* ptr);

void __http_request_free(http_request_t* ptr);
void __http_response_free(http_response_t* ptr);

void __ssl_close(ssl_context_t* ptr);

void __curl_slist_free_all(struct curl_slist** ptr);
void __curl_easy_cleanup(CURL** ptr);

#define __close__ __attribute__((__cleanup__(__close)))

#define __http_request_free__ __attribute__((__cleanup__(__http_request_free)))
#define __http_response_free__ __attribute__((__cleanup__(__http_response_free)))

#define __ssl_close__ __attribute__((__cleanup__(__ssl_close)))

#define __curl_slist_free_all__ __attribute__((__cleanup__(__curl_slist_free_all)))
#define __curl_easy_cleanup__ __attribute__((__cleanup__(__curl_easy_cleanup)))

#endif
