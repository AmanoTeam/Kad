#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
	#include <winsock2.h>
#else
	#include <sys/types.h>
#endif

#include <openssl/ssl.h>

#include "ssl.h"
#include "certificate.h"

static SSL_CTX* server_context = NULL;

static SSL_CTX* create_server_context(void) {

	int status = 1;

	if (server_context != NULL) {
		goto end;
	}

	server_context = SSL_CTX_new(TLS_server_method());

	if (server_context == NULL) {
		goto end;
	}

	SSL_CTX_set_mode(server_context, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_session_cache_mode(server_context, SSL_SESS_CACHE_SERVER);

	status = SSL_CTX_use_certificate_ASN1(server_context, sizeof(CERTIFICATE), CERTIFICATE);

	if (status != 1) {
		goto end;
	}

	status = SSL_CTX_use_RSAPrivateKey_ASN1(server_context, RSA_PRIVATE_KEY, sizeof(RSA_PRIVATE_KEY));

	end:;

	if (status != 1) {
		SSL_CTX_free(server_context);
		server_context = NULL;
	}

	return server_context;

}

int ssl_init(ssl_context_t* context, int* fd) {

	int err = 0;

	SSL_CTX* ctx = create_server_context();

	if (ctx == NULL) {
		err = -1;
		goto end;
	}

	context->ssl = SSL_new(ctx);

	if (context->ssl == NULL) {
		err = -1;
		goto end;
	}

	if (SSL_set_fd(context->ssl, *fd) != 1) {
		err = -1;
		goto end;
	}

	end:;

	if (err != 0) {
		SSL_free(context->ssl);
		context->ssl = NULL;
	}

	return err;

}

int ssl_accept(ssl_context_t* context, int* wait) {

	int status = 0;
	int error = 0;

	if (context->ssl == NULL) {
		return -1;
	}

	*wait = 0;

	status = SSL_accept(context->ssl);

	if (status == 1) {
		return 1;
	}

	error = SSL_get_error(context->ssl, status);

	switch (error) {
		case SSL_ERROR_WANT_READ: {
			*wait = SSL_WAIT_READ;
			return 0;
		}
		case SSL_ERROR_WANT_WRITE: {
			*wait = SSL_WAIT_WRITE;
			return 0;
		}
		default: {
			return -1;
		}
	}

}

static ssize_t secure_transfer(SSL* const ssl, char* const buffer, const size_t size, int* wait, const int writing) {

	size_t offset = 0;

	int status = 0;
	int error = 0;

	*wait = 0;

	while (offset < size) {
		status = writing
			? SSL_write(ssl, buffer + offset, (int) (size - offset))
			: SSL_read(ssl, buffer + offset, (int) (size - offset));

		if (status > 0) {
			offset += (size_t) status;
			continue;
		}

		error = SSL_get_error(ssl, status);

		switch (error) {
			case SSL_ERROR_WANT_READ: {
				*wait = SSL_WAIT_READ;
				break;
			}
			case SSL_ERROR_WANT_WRITE: {
				*wait = SSL_WAIT_WRITE;
				break;
			}
			case SSL_ERROR_ZERO_RETURN: {
				return writing ? -1 : -2;
			}
#if !defined(_WIN32)
			case SSL_ERROR_SYSCALL: {
				if (errno == EINTR) {
					continue;
				}
				return writing ? -1 : -2;
			}
#endif
			default: {
				return -1;
			}
		}

		return (ssize_t) offset;
	}

	return (ssize_t) offset;

}

ssize_t write_secure_chunk(ssl_context_t* context, const char* const buffer, const size_t size, int* wait) {

	if (context->ssl == NULL) {
		return -1;
	}

	return secure_transfer(context->ssl, (char*) buffer, size, wait, 1);

}

ssize_t read_secure_chunk(ssl_context_t* context, char* const buffer, const size_t size, int* wait) {

	if (context->ssl == NULL) {
		return -1;
	}

	return secure_transfer(context->ssl, buffer, size, wait, 0);

}

int ssl_close(ssl_context_t* context) {

	int status = 0;

	if (context->ssl == NULL) {
		return status;
	}

	status = SSL_shutdown(context->ssl);

	SSL_free(context->ssl);
	context->ssl = NULL;

	return status;

}
