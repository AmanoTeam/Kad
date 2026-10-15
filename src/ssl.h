#if !defined(SSL_H)
#define SSL_H

#include <stdlib.h>

#if !defined(_WIN32)
	#include <sys/types.h>
#endif

#include <openssl/ssl.h>

#define SSL_WAIT_READ (1)
#define SSL_WAIT_WRITE (2)

struct SSLContext {
	SSL* ssl;
};

typedef struct SSLContext ssl_context_t;

int ssl_init(ssl_context_t* context, int* fd);
int ssl_accept(ssl_context_t* context, int* wait);
ssize_t ssl_send(ssl_context_t* context, const char* const buffer, const size_t size, int* wait);
ssize_t ssl_recv(ssl_context_t* context, char* const buffer, const size_t size, int* wait);
int ssl_close(ssl_context_t* context);

#endif
