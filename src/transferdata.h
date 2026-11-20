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

struct curl_socket_context;

enum ClientState {
	CLIENT_STATE_NONE = 0,
	CLIENT_STATE_REQUEST = 1,
	CLIENT_STATE_HANDSHAKE = 2,
	CLIENT_STATE_FORWARD = 3,
	CLIENT_STATE_CLOSING = 4,
	CLIENT_STATE_WEBSOCKET = 5
};

typedef enum ClientState client_state_t;

#define TD_SECURE (1 << 0)
#define TD_EOF (1 << 1)
#define TD_FIN (1 << 2)
#define TD_PAUSE_UP (1 << 3)
#define TD_PAUSE_DOWN (1 << 4)
#define TD_DESTROY (1 << 5)
#define TD_WEBSOCKET (1 << 6)
#define TD_WS_ESTABLISHED (1 << 7)
#define TD_WS_PAUSE_UP (1 << 8)
#define TD_WS_PAUSE_DOWN (1 << 9)

struct transferdata {
	ssl_context_t context;
	http_object_t request;
	struct curl_slist* headers;
	CURL* curl;
#if defined(KAD_CAPTURE)
	capture_t capture;
#endif
	int fd;
	int flags;
	int wait;
	int events;
	int retries;
	bool retryable;
	client_state_t state;
	size_t remaining;
	size_t request_size;
	buffer_t buffer;
	buffer_t in;
	buffer_t out;
	buffer_t ws_pending;
	struct event* event;
	struct curl_socket_context* ws_context;
	char* hostname;
};

typedef struct transferdata transferdata_t;

#endif
