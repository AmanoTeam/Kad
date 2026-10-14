#if defined(KAD_CAPTURE)

#if !defined(CAPTURE_H)
#define CAPTURE_H

#include <stdlib.h>
#include <time.h>

#include <curl/curl.h>

#include "http.h"
#include "fstream.h"
#include "buffer.h"

struct capture {
	char* directory;
	char* url;
	const char* method;
	time_t time;
	fstream_t* request_body;
	fstream_t* response_body;
	buffer_t request_headers;
	int request_headers_done;
	char* response_headers;
	size_t response_headers_size;
	http_status_code_t status;
	int has_response;
	time_t response_time;
};

typedef struct capture capture_t;

void capture_init(capture_t* const capture, const http_request_t* const request);
void capture_request_headers(capture_t* const capture, const char* const buffer, const size_t size);
void capture_request_body(capture_t* const capture, const char* const buffer, const size_t size);
void capture_response_body(capture_t* const capture, const char* const buffer, const size_t size);
void capture_response(capture_t* const capture, const http_status_code_t status, const char* const headers, const size_t size);
void capture_end(capture_t* const capture, CURL* const curl);
void capture_close(capture_t* const capture);

#endif

#endif
