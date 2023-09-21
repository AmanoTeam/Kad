#include <curl/curl.h>

#if !defined(_WIN32)
	#include <unistd.h>
#endif

#include "cleanup.h"
#include "http.h"
#include "ssl.h"

void __curl_slist_free_all(struct curl_slist** ptr) {
	curl_slist_free_all(*ptr);
}

void __close(int* ptr) {
	close(*ptr);
}

void __http_request_free(struct HTTPRequest* ptr) {
	http_request_free(ptr);
}

void __http_response_free(struct HTTPResponse* ptr) {
	http_response_free(ptr);
}

void __ssl_close(struct SSLContext* ptr) {
	ssl_close(ptr);
}

void __curl_easy_cleanup(CURL** ptr) {
	curl_easy_cleanup(*ptr);
}