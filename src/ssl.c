#include <stdlib.h>

#if !defined(_WIN32)
	#include <sys/types.h>
#endif

#include <openssl/ssl.h>

#include "ssl.h"
#include "certificate.h"

int ssl_init(ssl_context_t* context, int* fd) {
	
	SSL_CTX* const ctx = SSL_CTX_new(TLS_server_method());
	
	if (ctx == NULL) {
		return -1;
	}
	
	if (SSL_CTX_use_certificate_ASN1(ctx, sizeof(CERTIFICATE), CERTIFICATE) != 1) {
		SSL_CTX_free(ctx);
		return -1;
	}
	
	if (SSL_CTX_use_RSAPrivateKey_ASN1(ctx, RSA_PRIVATE_KEY, sizeof(RSA_PRIVATE_KEY)) != 1) {
		SSL_CTX_free(ctx);
		return -1;
	}
	
	SSL* const ssl = SSL_new(ctx);
	
	if (ssl == NULL) {
		SSL_CTX_free(ctx);
		return -1;
	}
	
	SSL_set_fd(ssl, *fd);
	
	if (SSL_accept(ssl) != 1) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		return -1;
	}
	
	context->ctx = ctx;
	context->ssl = ssl;
	context->initialized = 1;
	
	return 0;
	
}

ssize_t ssl_send(ssl_context_t* context, const char* const buffer, const size_t size) {
	
	size_t offset = 0;
	
	while (offset < size) {
		const int wsize = SSL_write(context->ssl, buffer + offset, (int) (size - offset));
		
		if (wsize <= 0) {
			return -1;
		}
		
		offset += (size_t) wsize;
	}
	
	return (ssize_t) size;
	
}

ssize_t ssl_recv(ssl_context_t* context, char* const buffer, const size_t size) {
	
	const int rsize = SSL_read(context->ssl, buffer, (int) size);
	
	if (rsize <= 0) {
		return -1;
	}
	
	return (ssize_t) rsize;
	
}

int ssl_close(ssl_context_t* context) {
	
	if (!context->initialized) {
		return 0;
	}
	
	const int status = SSL_shutdown(context->ssl);
	
	SSL_free(context->ssl);
	SSL_CTX_free(context->ctx);
	
	context->ssl = NULL;
	context->ctx = NULL;
	context->initialized = 0;
	
	return status;
	
}
