#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <curl/curl.h>

#include "cleanup.h"
#include "callbacks.h"
#include "constants.h"
#include "transferdata.h"
#include "errors.h"
#include "logging.h"

size_t read_callback(char* dest, size_t size, size_t nmemb, void* userp) {
	
	transferdata_t* const data = (transferdata_t*) userp;
	
	if (data->remaining == 0) {
		return 0;
	}
	
	size_t offset = 0;
	const size_t requested = size * nmemb;
	
	if (data->request->body.size > 0) {
		size_t buffered = data->request->body.size;
		
		if (buffered > requested - offset) {
			buffered = requested - offset;
		}
		
		if (buffered > data->remaining) {
			buffered = data->remaining;
		}
		
		memcpy(dest + offset, data->request->body.content, buffered);
		
		memmove(data->request->body.content, data->request->body.content + buffered, data->request->body.size - buffered);
		data->request->body.size -= buffered;
		
		offset += buffered;
		data->remaining -= buffered;
	}
	
	while (data->remaining > 0 && offset < requested) {
		char chunk[MAX_CHUNK_SIZE];
		
		size_t wanted = sizeof(chunk);
		
		if (wanted > requested - offset) {
			wanted = requested - offset;
		}
		
		if (wanted > data->remaining) {
			wanted = data->remaining;
		}
		
		const ssize_t rsize = (data->is_secure) ? ssl_recv(context, chunk, wanted) : recv(data->fd, chunk, wanted, 0);
		
		if (rsize <= 0) {
			return CURL_READFUNC_ABORT;
		}
		
		memcpy(dest + offset, chunk, (size_t) rsize);
		
		offset += (size_t) rsize;
		data->remaining -= (size_t) rsize;
	}
	
	return offset;
	
}

size_t read_callback_empty(char* dest, size_t size, size_t nmemb, void* userp) {
	
	(void) size;
	(void) nmemb;
	(void) userp;
	
	return 0;
	
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userp) {
	
	transferdata_t* const data = (transferdata_t*) userp;
	ssl_context_t* context = &context;
	const size_t chunk_size = size * nmemb;
	
	const ssize_t wsize = (data->is_secure) ? ssl_send(context, ptr, chunk_size) : send(data->fd, ptr, chunk_size, 0);
	
	if (wsize == -1) {
		return CURL_WRITEFUNC_ERROR;
	}
	
	return chunk_size;
	
}

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
	
	transferdata_t* const data = (transferdata_t*) userdata;
	ssl_context_t* context = &context;
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
		http_response_t response __http_response_free__ = {0};
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
		
		ssize_t wsize = (data->is_secure) ? ssl_send(context, line, line_size) : send(data->fd, line, line_size, 0);
		
		if (wsize == -1) {
			return CURL_WRITEFUNC_ERROR;
		}
		
		for (size_t index = 0; index < response.headers.offset; index++) {
			const http_header_t* const header = &response.headers.items[index];
			
			// cURL already performs content decoding, so there is no need for these headers
			if (strcasecmp(header->key, "Content-Encoding") == 0) {
				continue;
			}
			
			if (strcasecmp(header->key, "Transfer-Encoding") == 0) {
				continue;
			}
			
			// This header will report an incorrect value for compressed/chunked responses, so let's just remove it
			if (strcasecmp(header->key, "Content-Length") == 0) {
				const http_header_t* const item = http_headers_get(&response.headers, "Transfer-Encoding");
				
				if (item != NULL && strcmp(item->value, "chunked") == 0) {
					continue;
				}
				
				const http_header_t* const subitem = http_headers_get(&response.headers, "Content-Encoding");
				
				if (subitem != NULL) {
					continue;
				}
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
			
			const ssize_t wsize = (data->is_secure) ? ssl_send(context, line, strlen(line)) : send(data->fd, line, strlen(line), 0);
			
			if (wsize == -1) {
				return CURL_WRITEFUNC_ERROR;
			}
		}
		
		wsize = (data->is_secure) ? ssl_send(context, CRLF, strlen(CRLF)) : send(data->fd, CRLF, strlen(CRLF), 0);
		
		if (wsize == -1) {
			return CURL_WRITEFUNC_ERROR;
		}
		
		buffer_free(&data->buffer);
		
		// the request is no longer needed once its body has been fully forwarded
		
		if (data->remaining == 0) {
			http_request_free(data->request);
		}
	}
	
	return nitems * size;
	
}