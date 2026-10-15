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

/* every tunneled connection shares the same SSL context, so the certificate
   and the private key are parsed only once and TLS sessions can be resumed
   instead of performing a full handshake for every single tunnel; this keeps
   the per-connection cost of the single-threaded event loop minimal */

static SSL_CTX* ssl_server_context(void) {

	static SSL_CTX* shared = NULL;

	if (shared != NULL) {
		return shared;
	}

	shared = SSL_CTX_new(TLS_server_method());

	if (shared == NULL) {
		return NULL;
	}

	/* the connections are event-driven on non-blocking sockets, so the
	   handshakes and the transfers may need to be resumed piecemeal */
	SSL_CTX_set_mode(shared, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	/* let the clients resume their TLS sessions across tunnels */
	SSL_CTX_set_session_cache_mode(shared, SSL_SESS_CACHE_SERVER);

	if (SSL_CTX_use_certificate_ASN1(shared, sizeof(CERTIFICATE), CERTIFICATE) != 1) {
		SSL_CTX_free(shared);

		shared = NULL;

		return NULL;
	}

	if (SSL_CTX_use_RSAPrivateKey_ASN1(shared, RSA_PRIVATE_KEY, sizeof(RSA_PRIVATE_KEY)) != 1) {
		SSL_CTX_free(shared);

		shared = NULL;

		return NULL;
	}

	return shared;

}

int ssl_init(ssl_context_t* context, int* fd) {

	int status = -1;

	SSL_CTX* ctx = NULL;
	SSL* ssl = NULL;

	ctx = ssl_server_context();

	if (ctx == NULL) {
		goto end;
	}

	ssl = SSL_new(ctx);

	if (ssl == NULL) {
		goto end;
	}

	SSL_set_fd(ssl, *fd);

	context->ssl = ssl;

	status = 0;

	end:;

	if (status != 0) {
		SSL_free(ssl);
	}

	return status;

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

ssize_t ssl_send(ssl_context_t* context, const char* const buffer, const size_t size, int* wait) {

	SSL* const ssl = context->ssl;

	size_t offset = 0;

	int wsize = 0;
	int error = 0;

	if (ssl == NULL) {
		return -1;
	}

	*wait = 0;

	while (offset < size) {
		wsize = SSL_write(ssl, buffer + offset, (int) (size - offset));

		if (wsize > 0) {
			offset += (size_t) wsize;
		} else {
			error = SSL_get_error(ssl, wsize);

			switch (error) {
				case SSL_ERROR_WANT_WRITE: {
					*wait = SSL_WAIT_WRITE;

					break;
				}
				case SSL_ERROR_WANT_READ: {
					*wait = SSL_WAIT_READ;

					break;
				}
#if !defined(_WIN32)
				case SSL_ERROR_SYSCALL: {
					if (errno != EINTR) {
						return -1;
					}

					continue;
				}
#endif
				default: {
					return -1;
				}
			}

			return (ssize_t) offset;
		}
	}

	return (ssize_t) offset;

}

ssize_t ssl_recv(ssl_context_t* context, char* const buffer, const size_t size, int* wait) {

	SSL* const ssl = context->ssl;

	size_t offset = 0;

	int rsize = 0;
	int error = 0;

	if (ssl == NULL) {
		return -1;
	}

	*wait = 0;

	while (offset < size) {
		rsize = SSL_read(ssl, buffer + offset, (int) (size - offset));

		if (rsize > 0) {
			offset += (size_t) rsize;
		} else {
			error = SSL_get_error(ssl, rsize);

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
					return -2;
				}
#if !defined(_WIN32)
				case SSL_ERROR_SYSCALL: {
					if (errno == EINTR) {
						continue;
					}

					return -2;
				}
#endif
				default: {
					return -1;
				}
			}

			return (ssize_t) offset;
		}
	}

	return (ssize_t) offset;

}

int ssl_close(ssl_context_t* context) {

	int status = 0;

	if (context->ssl == NULL) {
		return 0;
	}

	status = SSL_shutdown(context->ssl);

	SSL_free(context->ssl);

	context->ssl = NULL;

	return status;

}
