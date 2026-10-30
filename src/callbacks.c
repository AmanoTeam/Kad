#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include <curl/curl.h>

#include "cleanup.h"
#include "callbacks.h"
#include "client.h"
#include "constants.h"
#include "transferdata.h"
#include "errors.h"
#include "logging.h"

size_t read_callback(char* dest, size_t size, size_t nmemb, void* userp) {

	transferdata_t* const data = (transferdata_t*) userp;

	size_t buffered = 0;

	const size_t requested = size * nmemb;

	if (data->remaining == 0) {
		return 0;
	}

	if (data->request.body.size > 0) {
		buffered = data->request.body.size;

		if (buffered > requested) {
			buffered = requested;
		}

		if (buffered > data->remaining) {
			buffered = data->remaining;
		}

		memcpy(dest, data->request.body.content, buffered);

		data->retryable = false;

		#if defined(KAD_CAPTURE)
			capture_request_body(&data->capture, data->request.body.content, buffered);
		#endif

		memmove(data->request.body.content, data->request.body.content + buffered, data->request.body.size - buffered);
		data->request.body.size -= buffered;

		data->remaining -= buffered;

		return buffered;
	}

	/* the rest of the request body has not been received yet; pause the
	   transfer until the client sends more data */

	data->flags |= TD_PAUSE_UP;

	return CURL_READFUNC_PAUSE;

}

size_t read_callback_empty(char* dest, size_t size, size_t nmemb, void* userp) {

	(void) dest;
	(void) size;
	(void) nmemb;
	(void) userp;

	return 0;

}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userp) {

	transferdata_t* const data = (transferdata_t*) userp;

	const size_t chunk_size = size * nmemb;

	data->retryable = false;

	if (client_write(data, ptr, chunk_size) != 0) {
		return CURL_WRITEFUNC_ERROR;
	}

	#if defined(KAD_CAPTURE)
		capture_response_body(&data->capture, ptr, chunk_size);
	#endif

	return chunk_size;

}

#if defined(KAD_CAPTURE)
int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size, void* userp) {

	transferdata_t* const td = (transferdata_t*) userp;

	const char* prefix = NULL;

	/* cURL creates internal easy handles (e.g., for DoH) that inherit the
	   debug callback; ignore them. */

	if (handle != td->curl) {
		return 0;
	}

	if (type == CURLINFO_HEADER_OUT) {
		capture_request_headers(&td->capture, data, size);
	}

	/* replicate cURL's default verbose output, as it is now routed here
	   instead of being written to standard error */

	if (loglevel_get() == LOG_VERBOSE) {
		switch (type) {
			case CURLINFO_TEXT: {
				prefix = "* ";
				break;
			}
			case CURLINFO_HEADER_IN: {
				prefix = "< ";
				break;
			}
			case CURLINFO_HEADER_OUT: {
				prefix = "> ";
				break;
			}
			default: {
				prefix = NULL;
				break;
			}
		}

		if (prefix != NULL) {
			fwrite(prefix, 2, 1, stderr);
			fwrite(data, size, 1, stderr);
		}
	}

	return 0;

}
#endif

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {

	transferdata_t* const data = (transferdata_t*) userdata;

	http_response_t response __http_response_free__ = {0};

	int status = 0;

	const char* http_version = NULL;
	const char* message = NULL;

	const http_header_t* header = NULL;
	const http_header_t* item = NULL;
	const http_header_t* subitem = NULL;

	char* line = NULL;
	char* start = NULL;

	char status_code[3 + 1];

	size_t index = 0;

	const size_t chunk_size = nitems * size;

	size_t line_size = 0;

	int code = 0;

	const size_t slength = data->buffer.slength + chunk_size;

	data->buffer.s = realloc(data->buffer.s, slength + 1);

	if (data->buffer.s == NULL) {
		status = -1;
		goto end;
	}

	memcpy(data->buffer.s + data->buffer.slength, buffer, chunk_size);

	data->buffer.s[slength] = '\0';
	data->buffer.slength = slength;

	if (data->buffer.slength > strlen(CRLFCRLF) && memcmp(data->buffer.s + (data->buffer.slength - strlen(CRLFCRLF)), CRLFCRLF, strlen(CRLFCRLF)) == 0) {
		http_response_init(&response);

		code = http_response_parse(&response, data->buffer.s, data->buffer.slength);

		if (code != KADERR_SUCCESS) {
			status = -1;
			goto end;
		}

		#if defined(KAD_CAPTURE)
			capture_response(&data->capture, response.status, data->buffer.s, data->buffer.slength);
		#endif

		http_version = http_version_stringify(HTTP11);
		message = http_status_stringify(response.status);

		if (message == NULL) {
			message = "Unknown";
		}

		sprintf(status_code, "%i", (int) response.status);

		line_size = strlen(PROTOCOL_NAME) + strlen(SLASH) + strlen(http_version) + strlen(SPACE) + strlen(status_code) + strlen(SPACE) + strlen(message) + strlen(CRLF);

		line = malloc(line_size + 1);

		if (line == NULL) {
			status = -1;
			goto end;
		}

		strcpy(line, PROTOCOL_NAME);
		strcat(line, SLASH);
		strcat(line, http_version);
		strcat(line, SPACE);
		strcat(line, status_code);
		strcat(line, SPACE);
		strcat(line, message);
		strcat(line, CRLF);

		data->retryable = false;

		if (client_write(data, line, line_size) != 0) {
			free(line);
			line = NULL;

			status = -1;
			goto end;
		}

		free(line);
		line = NULL;

		/* the connection closes after each response, so the client must not
		   pool it and reuse it while it is already gone; the upstream's
		   Connection header is stripped below, so it is set explicitly here */

		if (client_write(data, "Connection: close\r\n", 19) != 0) {
			status = -1;
			goto end;
		}

		for (index = 0; index < response.headers.offset; index++) {
			header = &response.headers.items[index];

			/* cURL already performs content decoding, so there is no need for these headers */
			if (strcasecmp(header->key, "Content-Encoding") == 0) {
				continue;
			}

			if (strcasecmp(header->key, "Transfer-Encoding") == 0) {
				continue;
			}

			/* This header will report an incorrect value for compressed/chunked responses, so let's just remove it */
			if (strcasecmp(header->key, "Content-Length") == 0) {
				item = http_headers_get(&response.headers, "Transfer-Encoding");

				if (item != NULL && strcmp(item->value, "chunked") == 0) {
					continue;
				}

				subitem = http_headers_get(&response.headers, "Content-Encoding");

				if (subitem != NULL) {
					continue;
				}
			}

			if (strcasecmp(header->key, "Connection") == 0) {
				continue;
			}

			line = malloc(strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + strlen(CRLF) + 1);

			if (line == NULL) {
				status = -1;
				goto end;
			}

			strcpy(line, header->key);

			if (response.version >= HTTP2) {
				*line = (char) toupper(line[0]);

				start = line;

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

			if (client_write(data, line, strlen(line)) != 0) {
				free(line);
				line = NULL;

				status = -1;
				goto end;
			}

			free(line);
			line = NULL;
		}

		if (client_write(data, CRLF, strlen(CRLF)) != 0) {
			status = -1;
			goto end;
		}

		buffer_free(&data->buffer);

		/* the request is no longer needed once its body has been fully forwarded */

		if (data->remaining == 0) {
			http_request_free(&data->request);
		}
	}

	end:;

	if (line != NULL) {
		free(line);
	}

	if (status != 0) {
		return CURL_WRITEFUNC_ERROR;
	}

	return nitems * size;

}
