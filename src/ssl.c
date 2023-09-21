#include <stdlib.h>

#if !defined(_WIN32)
	#include <sys/types.h>
#endif

#include <bearssl.h>

#include "ssl.h"
#include "certificate.h"
#include "callbacks.h"

int ssl_init(struct SSLContext* context, int* fd) {
	
	br_ssl_server_init_full_rsa(&context->server_context, CHAIN, CHAIN_LEN, &RSA);
	br_ssl_engine_set_buffer(&context->server_context.eng, context->io, sizeof(context->io), 1);
	
	if (br_ssl_server_reset(&context->server_context) == 0) {
		return -1;
	}
	
	br_sslio_init(&context->io_context, &context->server_context.eng, sock_read, fd, sock_write, fd);
	
	context->initialized = 1;
	
	return 0;
	
}

ssize_t ssl_send(struct SSLContext* context, const char* const buffer, const size_t size) {
	
	const int status = br_sslio_write_all(&context->io_context, buffer, size);
	
	if (status == 0) {
		return (ssize_t) size;
	}
	
	return (ssize_t) status;
	
}

ssize_t ssl_recv(struct SSLContext* context, char* const buffer, const size_t size) {
	
	const ssize_t rsize = (ssize_t) br_sslio_read(&context->io_context, buffer, size);
	return rsize;
	
}

int ssl_close(struct SSLContext* context) {
	
	if (!context->initialized) {
		return 0;
	}
	
	const int status = br_sslio_close(&context->io_context);
	return status;
	
}