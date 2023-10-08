#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include <curl/curl.h>

#if defined(_WIN32)
	#include <io.h>
#endif

#if !defined(_WIN32)
	#include <unistd.h>
	#include <netdb.h>
#endif

#include "cleanup.h"
#include "callbacks.h"
#include "constants.h"
#include "transferdata.h"
#include "errors.h"

int sock_write(void* fd, const unsigned char *buffer, size_t buffer_size) {
	
	for (;;) {
		const ssize_t wlen = write(*(int*) fd, buffer, buffer_size);
		
		if (wlen < 0 && errno == EINTR) {
			continue;
		}
		
		if (wlen == 0) {
			return -1;
		}
		
		return (int) wlen;
	}
	
}

int sock_read(void* fd, unsigned char* buffer, size_t buffer_size) {
	
	for (;;) {
		const ssize_t rlen = read(*(int*) fd, buffer, buffer_size);
		
		if (rlen < 0 && errno == EINTR) {
			continue;
		}
		
		if (rlen == 0) {
			return -1;
		}
		
		return (int) rlen;
	}
	
}

size_t read_callback(char *dest, size_t size, size_t nmemb, void *userp) {
	
	(void) size;
	(void) nmemb;
	
	struct transferdata* const data = (struct transferdata*) userp;
	
	if (data->eof) {
		return 0;
	}
	
	const size_t body_size = data->request->body.size;
	
	if (body_size > 0) {
		memcpy(dest, data->request->body.content, body_size);
		data->request->body.size = 0;
		data->eof = 1;
		
		return body_size;
	}
	
	char chunk[MAX_CHUNK_SIZE];
	const ssize_t rsize = (data->is_secure) ? ssl_recv(data->context, chunk, sizeof(chunk)) : recv(data->fd, chunk, sizeof(chunk), 0);
	
	if (rsize == -1) {
		return CURL_READFUNC_ABORT;
	}
	
	memcpy(dest, chunk, (size_t) rsize);
	
	data->eof = 1;
	
	return (size_t) rsize;
	
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userp) {
	
	struct transferdata* const data = (struct transferdata*) userp;
	const size_t chunk_size = size * nmemb;
	
	const ssize_t wsize = (data->is_secure) ? ssl_send(data->context, ptr, chunk_size) : send(data->fd, ptr, chunk_size, 0);
	
	if (wsize == -1) {
		return CURL_WRITEFUNC_ERROR;
	}
	
	return chunk_size;
	
}

size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
	
	struct transferdata* const data = (struct transferdata*) userdata;
	const size_t chunk_size = nitems * size;
	
	const size_t slength = data->buffer.slength + chunk_size;
	data->buffer.s = realloc(data->buffer.s, slength + 1);
	
	if (data->buffer.s == NULL) {
		return CURL_WRITEFUNC_ERROR;
	}
	
	memcpy(data->buffer.s + data->buffer.slength, buffer, chunk_size);
	
	data->buffer.s[slength] = '\0';
	data->buffer.slength = slength;
	
	if (data->buffer.slength > strlen(CRLFCRLF) && memcmp(data->buffer.s + (data->buffer.slength - strlen(CRLFCRLF)), CRLFCRLF, strlen(CRLFCRLF)) == 0) {
		struct HTTPResponse response __http_response_free__ = {0};
		http_response_init(&response);
		
		int code = http_response_parse(&response, data->buffer.s, data->buffer.slength);
		
		if (code != KADERR_SUCCESS) {
			return CURL_WRITEFUNC_ERROR;
		}
		
		const char* const http_version = http_version_stringify(HTTP10);
		const char* const message = http_status_stringify(response.status);
		
		char status_code[3 + 1];
		snprintf(status_code, sizeof(status_code), "%i", (int) response.status);
		
		const size_t line_size = strlen(PROTOCOL_NAME) + strlen(SLASH) + strlen(http_version) + strlen(SPACE) + strlen(status_code) + strlen(SPACE) + strlen(message) + strlen(CRLF);
		
		char line[line_size + 1];
		
		strcpy(line, PROTOCOL_NAME);
		strcat(line, SLASH);
		strcat(line, http_version);
		strcat(line, SPACE);
		strcat(line, status_code);
		strcat(line, SPACE);
		strcat(line, message);
		strcat(line, CRLF);
		
		ssize_t wsize = (data->is_secure) ? ssl_send(data->context, line, line_size) : send(data->fd, line, line_size, 0);
		
		if (wsize == -1) {
			return CURL_WRITEFUNC_ERROR;
		}
		
		for (size_t index = 0; index < response.headers.offset; index++) {
			const struct HTTPHeader* const header = &response.headers.items[index];
			
			// cURL already performs content decoding, so there is no need for these headers
			if (strcasecmp(header->key, "Content-Encoding") == 0) {
				continue;
			}
			
			if (strcasecmp(header->key, "Transfer-Encoding") == 0) {
				continue;
			}
			
			// This header will report an incorrect value for compressed responses, so let's just remove it
			if (strcasecmp(header->key, "Content-Length") == 0) {
				continue;
			}
			
			// Kad doesn't support persistent connections, and the HTTP/1.0 protocol already assumes a short-lived connection by default
			if (strcasecmp(header->key, "Connection") == 0) {
				continue;
			}
			
			char line[strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + strlen(CRLF) + 1];
			strcpy(line, header->key);
			
			if (response.version >= HTTP2) {
				*line = (char) toupper(line[0]);
				
				char* start = line;
				
				while (1) {
					start = strstr(start, "-");
					
					if (start == NULL) {
						break;
					}
					
					start += 1;
					
					*start = (char) toupper(start[0]);
				}
			}
			
			strcat(line, HEADER_SEPARATOR);
			strcat(line, header->value);
			strcat(line, CRLF);
			
			const ssize_t wsize = (data->is_secure) ? ssl_send(data->context, line, strlen(line)) : send(data->fd, line, strlen(line), 0);
			
			if (wsize == -1) {
				return CURL_WRITEFUNC_ERROR;
			}
		}
		
		wsize = (data->is_secure) ? ssl_send(data->context, CRLF, strlen(CRLF)) : send(data->fd, CRLF, strlen(CRLF), 0);
		
		if (wsize == -1) {
			return CURL_WRITEFUNC_ERROR;
		}
		
		buffer_free(&data->buffer);
	}
	
	 return nitems * size;
	
}