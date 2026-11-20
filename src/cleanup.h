#if !defined(CLEANUP_H)
#define CLEANUP_H

#include <curl/curl.h>

#include "http.h"
#include "ssl.h"
#include "transferdata.h"

void __close(int* ptr);

void __http_object_free(http_object_t* ptr);

void __ssl_close(ssl_context_t* ptr);

void __curl_slist_free_all(struct curl_slist** ptr);
void __curl_easy_cleanup(CURL** ptr);

void transferdata_close(transferdata_t* const data);
void __transferdata_close(transferdata_t** ptr);

#define __close__ __attribute__((__cleanup__(__close)))

#define __transferdata_close__ __attribute__((__cleanup__(__transferdata_close)))

#define __http_object_free__ __attribute__((__cleanup__(__http_object_free)))

#define __ssl_close__ __attribute__((__cleanup__(__ssl_close)))

#define __curl_slist_free_all__ __attribute__((__cleanup__(__curl_slist_free_all)))
#define __curl_easy_cleanup__ __attribute__((__cleanup__(__curl_easy_cleanup)))

#endif
