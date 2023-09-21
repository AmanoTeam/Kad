#include <stdlib.h>

#if !defined(_WIN32)
	#include <sys/types.h>
#endif

#include <bearssl.h>

struct SSLContext {
	br_ssl_server_context server_context;
	br_sslio_context io_context;
	unsigned char io[BR_SSL_BUFSIZE_BIDI];
	int initialized;
};

int ssl_init(struct SSLContext* context, int* fd);
ssize_t ssl_send(struct SSLContext* context, const char* const buffer, const size_t size);
ssize_t ssl_recv(struct SSLContext* context, char* const buffer, const size_t size);
int ssl_close(struct SSLContext* context);

#pragma once
