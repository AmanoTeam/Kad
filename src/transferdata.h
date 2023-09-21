#include "ssl.h"
#include "http.h"
#include "buffer.h"

struct transferdata {
	struct SSLContext* context;
	struct HTTPRequest* request;
	struct HTTPResponse* response;
	int fd;
	int is_secure;
	int eof;
	buffer_t buffer;
};

#pragma once
