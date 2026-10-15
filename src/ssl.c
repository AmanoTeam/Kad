#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
	#include <winsock2.h>
#else
	#include <sys/types.h>
	#include <sys/select.h>
#endif

#include <openssl/ssl.h>

#include "ssl.h"
#include "certificate.h"

#define SSL_WAIT_READ (1)
#define SSL_WAIT_WRITE (2)

static int ssl_wait(const int fd, const int events) {
	
	fd_set readfds;
	fd_set writefds;
	
	int status = 0;
	
	if (fd < 0) {
		return -1;
	}
	
#if !defined(_WIN32)
	if (fd >= FD_SETSIZE) {
		return -1;
	}
#endif
	
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	
	while (1) {
		if ((events & SSL_WAIT_READ) != 0) {
			FD_SET(fd, &readfds);
		}
	
		if ((events & SSL_WAIT_WRITE) != 0) {
			FD_SET(fd, &writefds);
		}
	
		status = select(fd + 1, ((events & SSL_WAIT_READ) != 0) ? &readfds : NULL, ((events & SSL_WAIT_WRITE) != 0) ? &writefds : NULL, NULL, NULL);
	
		if (status > 0) {
			return 0;
		}
	
#if !defined(_WIN32)
		if (status == -1 && errno == EINTR) {
			continue;
		}
#endif
	
		return -1;
	}
	
}

int ssl_init(ssl_context_t* context, int* fd) {
	
	int status = -1;
	
	SSL_CTX* ctx = NULL;
	SSL* ssl = NULL;
	
	ctx = SSL_CTX_new(TLS_server_method());
	
	if (ctx == NULL) {
		goto end;
	}
	
	if (SSL_CTX_use_certificate_ASN1(ctx, sizeof(CERTIFICATE), CERTIFICATE) != 1) {
		goto end;
	}
	
	if (SSL_CTX_use_RSAPrivateKey_ASN1(ctx, RSA_PRIVATE_KEY, sizeof(RSA_PRIVATE_KEY)) != 1) {
		goto end;
	}
	
	ssl = SSL_new(ctx);
	
	if (ssl == NULL) {
		goto end;
	}
	
	SSL_set_fd(ssl, *fd);
	
	if (SSL_accept(ssl) != 1) {
		goto end;
	}
	
	context->ctx = ctx;
	context->ssl = ssl;
	context->initialized = 1;
	
	status = 0;
	
	end:;
	
	if (status != 0) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
	}
	
	return status;
	
}

ssize_t ssl_send(ssl_context_t* context, const char* const buffer, const size_t size) {
	
	SSL* const ssl = context->ssl;
	
	const int fd = SSL_get_fd(ssl);
	
	size_t offset = 0;
	int sslerror = 0;
	int syserror = 0;
	
	while (offset < size) {
		const int wsize = SSL_write(ssl, buffer + offset, (int) (size - offset));
	
		if (wsize > 0) {
			offset += (size_t) wsize;
		} else {
			syserror = errno;
			sslerror = SSL_get_error(ssl, wsize);
	
			switch (sslerror) {
				case SSL_ERROR_WANT_WRITE: {
					if (ssl_wait(fd, SSL_WAIT_WRITE) == -1) {
						return -1;
					}
	
					break;
				}
				case SSL_ERROR_WANT_READ: {
					if (ssl_wait(fd, SSL_WAIT_READ) == -1) {
						return -1;
					}
	
					break;
				}
#if !defined(_WIN32)
				case SSL_ERROR_SYSCALL: {
					if (syserror != EINTR) {
						return -1;
					}
	
					break;
				}
#endif
				default: {
					return -1;
				}
			}
		}
	}
	
	return (ssize_t) size;
	
}

ssize_t ssl_recv(ssl_context_t* context, char* const buffer, const size_t size) {
	
	SSL* const ssl = context->ssl;
	
	const int fd = SSL_get_fd(ssl);
	
	int sslerror = 0;
	int syserror = 0;
	
	while (1) {
		const int rsize = SSL_read(ssl, buffer, (int) size);
	
		if (rsize > 0) {
			return (ssize_t) rsize;
		}
	
		syserror = errno;
		sslerror = SSL_get_error(ssl, rsize);
	
		switch (sslerror) {
			case SSL_ERROR_WANT_READ: {
				if (ssl_wait(fd, SSL_WAIT_READ) == -1) {
					return -1;
				}
	
				break;
			}
			case SSL_ERROR_WANT_WRITE: {
				if (ssl_wait(fd, SSL_WAIT_WRITE) == -1) {
					return -1;
				}
	
				break;
			}
#if !defined(_WIN32)
			case SSL_ERROR_SYSCALL: {
				if (syserror != EINTR) {
					return -1;
				}
	
				break;
			}
#endif
			default: {
				return -1;
			}
		}
	}
	
}

int ssl_close(ssl_context_t* context) {
	
	int status = 0;
	
	if (!context->initialized) {
		return 0;
	}
	
	status = SSL_shutdown(context->ssl);
	
	SSL_free(context->ssl);
	SSL_CTX_free(context->ctx);
	
	context->ssl = NULL;
	context->ctx = NULL;
	context->initialized = 0;
	
	return status;
	
}
