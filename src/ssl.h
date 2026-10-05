#if !defined(SSL_H)
#define SSL_H

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

typedef struct SSLContext ssl_context_t;

int ssl_init(ssl_context_t* context, int* fd);
ssize_t ssl_send(ssl_context_t* context, const char* const buffer, const size_t size);
ssize_t ssl_recv(ssl_context_t* context, char* const buffer, const size_t size);
int ssl_close(ssl_context_t* context);

#endif
