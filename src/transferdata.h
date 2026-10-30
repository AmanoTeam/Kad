#if !defined(TRANSFERDATA_H)
#define TRANSFERDATA_H

#include <stdlib.h>
#include <stdbool.h>

#include <curl/curl.h>

#include "ssl.h"
#include "http.h"
#include "buffer.h"

#if defined(KAD_CAPTURE)
	#include "capture.h"
#endif

struct event;

enum ClientState {
	CLIENT_STATE_NONE = 0,
	CLIENT_STATE_REQUEST = 1,
	CLIENT_STATE_HANDSHAKE = 2,
	CLIENT_STATE_FORWARD = 3,
	CLIENT_STATE_CLOSING = 4
};

typedef enum ClientState client_state_t;

struct transferdata {
	ssl_context_t context;
	http_request_t request;
	struct curl_slist* headers;
	CURL* curl;
#if defined(KAD_CAPTURE)
	capture_t capture;
#endif
	int fd;
	int is_secure;
	int read_eof;
	int fin_sent;
	int wait;
	int upload_paused;
	int download_paused;
	int destroy_pending;
	int events;
	int retries;
	bool retryable;
	client_state_t state;
	size_t remaining;
	size_t request_size;
	buffer_t buffer;
	buffer_t in;
	buffer_t out;
	struct event* event;
	char* hostname;
};

typedef struct transferdata transferdata_t;

#endif
