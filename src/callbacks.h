#if !defined(CALLBACKS_H)
#define CALLBACKS_H

#include <stdlib.h>

#if defined(KAD_CAPTURE)
	#include <curl/curl.h>
#endif

size_t read_callback(char* dest, size_t size, size_t nmemb, void* userp);
size_t read_callback_empty(char* dest, size_t size, size_t nmemb, void* userp);
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userp);
size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);

#if defined(KAD_CAPTURE)
	int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size, void* userp);
#endif

#endif
