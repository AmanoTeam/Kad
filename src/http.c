#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <picohttpparser.h>

#include "http.h"
#include "errors.h"
#include "constants.h"

#define MAX_HTTP_HEADERS_COUNT (128)

const char* http_method_stringify(const http_method_t method) {
	
	switch (method) {
		case GET: {
			return "GET";
		}
		case HEAD: {
			return "HEAD";
		}
		case POST: {
			return "POST";
		}
		case PUT: {
			return "PUT";
		}
		case DELETE: {
			return "DELETE";
		}
		case CONNECT: {
			return "CONNECT";
		}
		case OPTIONS: {
			return "OPTIONS";
		}
		case TRACE: {
			return "TRACE";
		}
	}
	
	return NULL;
	
}

const char* http_version_stringify(const http_version_t version) {
	
	switch (version) {
		case HTTP10: {
			return "1.0";
		}
		case HTTP11: {
			return "1.1";
		}
		case HTTP2: {
			return "2";
		}
	}
	
	return NULL;
	
}

const char* http_status_stringify(const http_status_code_t status_code) {
	
	switch (status_code) {
		case CONTINUE: {
			return "Continue";
		}
		case SWITCHING_PROTOCOLS: {
			return "Switching Protocols";
		}
		case PROCESSING: {
			return "Processing";
		}
		case EARLY_HINTS: {
			return "Early Hints";
		}
		case OK: {
			return "OK";
		}
		case CREATED: {
			return "Created";
		}
		case ACCEPTED: {
			return "Accepted";
		}
		case NON_AUTHORITATIVE_INFORMATION: {
			return "Non-Authoritative Information";
		}
		case NO_CONTENT: {
			return "No Content";
		}
		case RESET_CONTENT: {
			return "Reset Content";
		}
		case PARTIAL_CONTENT: {
			return "Partial Content";
		}
		case MULTI_STATUS: {
			return "Multi-Status";
		}
		case ALREADY_REPORTED: {
			return "Already Reported";
		}
		case IM_USED: {
			return "IM Used";
		}
		case MULTIPLE_CHOICES: {
			return "Multiple Choices";
		}
		case MOVED_PERMANENTLY: {
			return "Moved Permanently";
		}
		case FOUND: {
			return "Found";
		}
		case SEE_OTHER: {
			return "See Other";
		}
		case NOT_MODIFIED: {
			return "Not Modified";
		}
		case USE_PROXY: {
			return "Use Proxy";
		}
		case TEMPORARY_REDIRECT: {
			return "Temporary Redirect";
		}
		case PERMANENT_REDIRECT: {
			return "Permanent Redirect";
		}
		case BAD_REQUEST: {
			return "Bad Request";
		}
		case UNAUTHORIZED: {
			return "Unauthorized";
		}
		case PAYMENT_REQUIRED: {
			return "Payment Required";
		}
		case FORBIDDEN: {
			return "Forbidden";
		}
		case NOT_FOUND: {
			return "Not Found";
		}
		case METHOD_NOT_ALLOWED: {
			return "Method Not Allowed";
		}
		case NOT_ACCEPTABLE: {
			return "Not Acceptable";
		}
		case PROXY_AUTHENTICATION_REQUIRED: {
			return "Proxy Authentication Required";
		}
		case REQUEST_TIMEOUT: {
			return "Request Timeout";
		}
		case CONFLICT: {
			return "Conflict";
		}
		case GONE: {
			return "Gone";
		}
		case LENGTH_REQUIRED: {
			return "Length Required";
		}
		case PRECONDITION_FAILED: {
			return "Precondition Failed";
		}
		case REQUEST_ENTITY_TOO_LARGE: {
			return "Request Entity Too Large";
		}
		case REQUEST_URI_TOO_LONG: {
			return "Request-URI Too Long";
		}
		case UNSUPPORTED_MEDIA_TYPE: {
			return "Unsupported Media Type";
		}
		case REQUESTED_RANGE_NOT_SATISFIABLE: {
			return "Requested Range Not Satisfiable";
		}
		case EXPECTATION_FAILED: {
			return "Expectation Failed";
		}
		case IM_A_TEAPOT: {
			return "I'm a Teapot";
		}
		case MISDIRECTED_REQUEST: {
			return "Misdirected Request";
		}
		case UNPROCESSABLE_ENTITY: {
			return "Unprocessable Entity";
		}
		case LOCKED: {
			return "Locked";
		}
		case FAILED_DEPENDENCY: {
			return "Failed Dependency";
		}
		case TOO_EARLY: {
			return "Too Early";
		}
		case UPGRADE_REQUIRED: {
			return "Upgrade Required";
		}
		case PRECONDITION_REQUIRED: {
			return "Precondition Required";
		}
		case TOO_MANY_REQUESTS: {
			return "Too Many Requests";
		}
		case REQUEST_HEADER_FIELDS_TOO_LARGE: {
			return "Request Header Fields Too Large";
		}
		case UNAVAILABLE_FOR_LEGAL_REASONS: {
			return "Unavailable For Legal Reasons";
		}
		case INTERNAL_SERVER_ERROR: {
			return "Internal Server Error";
		}
		case NOT_IMPLEMENTED: {
			return "Not Implemented";
		}
		case BAD_GATEWAY: {
			return "Bad Gateway";
		}
		case SERVICE_UNAVAILABLE: {
			return "Service Unavailable";
		}
		case GATEWAY_TIMEOUT: {
			return "Gateway Timeout";
		}
		case HTTP_VERSION_NOT_SUPPORTED: {
			return "HTTP Version Not Supported";
		}
		case VARIANT_ALSO_NEGOTIATES: {
			return "Variant Also Negotiates";
		}
		case INSUFFICIENT_STORAGE: {
			return "Insufficient Storage";
		}
		case LOOP_DETECTED: {
			return "Loop Detected";
		}
		case NOT_EXTENDED: {
			return "Not Extended";
		}
		case NETWORK_AUTHENTICATION_REQUIRED: {
			return "Network Authentication Required";
		}
	}
	
	return NULL;
	
}

int http_headers_add(http_headers_t* const headers, const char* key, const size_t key_size, const char* value, const size_t value_size) {
	
	int status = 0;
	
	http_header_t header = {NULL, NULL};
	
	size_t size = 0;
	http_header_t* items = NULL;
	
	if (headers->offset < headers->capacity) {
		items = headers->items;
	} else {
		size = ((headers->capacity == 0) ? 8 : (headers->capacity * 2));
		
		items = (http_header_t*) realloc(headers->items, (size * sizeof(http_header_t)));
		
		if (items == NULL) {
			status = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		headers->capacity = size;
		headers->items = items;
	}
	
	header.key = malloc(key_size + value_size + 2);
	
	if (header.key == NULL) {
		status = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	memcpy(header.key, ((key == NULL) ? "" : key), key_size);
	header.key[key_size] = '\0';
	
	header.value = (header.key + key_size + 1);
	
	memcpy(header.value, ((value == NULL) ? "" : value), value_size);
	header.value[value_size] = '\0';
	
	headers->items[headers->offset++] = header;
	
	end:;
	
	if (status != KADERR_SUCCESS) {
		free(header.key);
		header.key = NULL;
	}
	
	return status;
	
}

static int http_headers_reserve(http_headers_t* const headers, const size_t count) {
	
	int status = 0;
	
	http_header_t* items = NULL;
	
	if (headers->capacity >= count) {
		return status;
	}
	
	items = (http_header_t*) realloc(headers->items, (count * sizeof(http_header_t)));
	
	if (items == NULL) {
		status = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	headers->capacity = count;
	headers->items = items;
	
	end:;
	
	return status;
	
}

const http_header_t* http_headers_get(const http_headers_t* const headers, const char* key) {
	
	size_t index = 0;
	
	const http_header_t* header = NULL;
	
	for (index = 0; index < headers->offset; index++) {
		header = &headers->items[index];
		
		if (strcasecmp(header->key, key) == 0) {
			return header;
		}
	}
	
	return NULL;
	
}

static enum HTTPMethod http_method_from_string(const char* const method, const size_t size) {
	
	size_t index = 0;
	
	static const char* const methods[] = {
		"GET",
		"HEAD",
		"POST",
		"PUT",
		"DELETE",
		"CONNECT",
		"OPTIONS",
		"TRACE"
	};
	
	for (index = 0; index < sizeof(methods) / sizeof(methods[0]); index++) {
		if (strlen(methods[index]) == size && memcmp(methods[index], method, size) == 0) {
			return (http_method_t) (index + 1);
		}
	}
	
	return (http_method_t) 0;
	
}

void http_request_init(http_request_t* const request) {
	memset(request, 0, sizeof(*request));
	request->type = HTTP_REQUEST;
}

void http_response_init(http_response_t* const response) {
	memset(response, 0, sizeof(*response));
	response->type = HTTP_RESPONSE;
}

int http_request_parse(http_request_t* const object, const char* const buffer, const size_t size) {
	return http_object_parse((http_object_t*) object, buffer, size);
}

int http_response_parse(http_response_t* const object, const char* const buffer, const size_t size) {
	return http_object_parse((http_object_t*) object, buffer, size);
}

int http_object_parse(http_object_t* const object, const char* const buffer, const size_t size) {
	
	const char* parse_buffer = buffer;
	size_t parse_size = size;
	
	const char* method = NULL;
	size_t method_size = 0;
	
	const char* path = NULL;
	size_t path_size = 0;
	
	const char* message = NULL;
	size_t message_size = 0;
	
	char* patched_buffer = NULL;
	
	struct phr_header headers[MAX_HTTP_HEADERS_COUNT];
	
	size_t headers_count = 0;
	
	size_t offset_delta = 0;
	
	size_t index = 0;
	
	size_t headers_size = 0;
	size_t body_size = 0;
	
	int is_http2 = 0;
	
	int status = 0;
	int minor_version = -1;
	
	int consumed = 0;
	int code = 0;
	
	if (object->type == HTTP_REQUEST && size > (size_t) MAX_HTTP_HEADERS_SIZE) {
		code = KADERR_HTTP_HEADERS_TOO_BIG;
		goto end;
	}
	
	headers_count = MAX_HTTP_HEADERS_COUNT;
	
	/* cURL reports HTTP/2 responses with an "HTTP/2 <code>" status line, which the parser doesn't understand, so we rewrite it as HTTP/1.1 and adjust for the 2 extra bytes later */
	if (object->type == HTTP_RESPONSE && size > strlen("HTTP/2 ") && memcmp(buffer, "HTTP/2", strlen("HTTP/2")) == 0 && (buffer[strlen("HTTP/2")] == ' ' || buffer[strlen("HTTP/2")] == '\r')) {
		patched_buffer = malloc(size + 2);
		
		if (patched_buffer == NULL) {
			code = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		memcpy(patched_buffer, "HTTP/1.1", strlen("HTTP/1.1"));
		memcpy(patched_buffer + strlen("HTTP/1.1"), buffer + strlen("HTTP/2"), size - strlen("HTTP/2"));
		
		parse_buffer = patched_buffer;
		parse_size = size + 2;
		offset_delta = 2;
		
		is_http2 = 1;
	}
	
	consumed = (
		(object->type == HTTP_REQUEST)
			? phr_parse_request(parse_buffer, parse_size, &method, &method_size, &path, &path_size, &minor_version, headers, &headers_count, 0)
			: phr_parse_response(parse_buffer, parse_size, &minor_version, &status, &message, &message_size, headers, &headers_count, 0)
	);
	
	if (consumed < 0) {
		code = KADERR_HTTP_MALFORMED_REQUEST;
		goto end;
	}
	
	if (headers_count == MAX_HTTP_HEADERS_COUNT) {
		code = KADERR_HTTP_HEADERS_TOO_BIG;
		goto end;
	}
	
	code = KADERR_SUCCESS;
	
	code = http_headers_reserve(&object->headers, headers_count);
	
	if (code != KADERR_SUCCESS) {
		goto end;
	}
	
	code = KADERR_SUCCESS;
	
	switch (minor_version) {
		case 0: {
			object->version = HTTP10;
			break;
		}
		case 1: {
			object->version = HTTP11;
			break;
		}
		default: {
			code = KADERR_HTTP_UNSUPPORTED_VERSION;
			break;
		}
	}
	
	if (is_http2) {
		object->version = HTTP2;
	}
	
	if (code == KADERR_SUCCESS && object->type == HTTP_REQUEST) {
		object->method = http_method_from_string(method, method_size);
		
		if (object->method == 0) {
			code = KADERR_HTTP_UNKNOWN_METHOD;
		}
	}
	
	if (code == KADERR_SUCCESS && object->type == HTTP_REQUEST) {
		object->uri = malloc(path_size + 1);
		
		if (object->uri == NULL) {
			code = KADERR_MEMORY_ALLOCATE_FAILURE;
		} else {
			memcpy(object->uri, path, path_size);
			object->uri[path_size] = '\0';
		}
	}
	
	if (code == KADERR_SUCCESS && object->type == HTTP_RESPONSE) {
		object->status = (http_status_code_t) status;
	}
	
	if (code == KADERR_SUCCESS) {
		for (index = 0; index < headers_count; index++) {
			const struct phr_header* const header = &headers[index];
			
			code = http_headers_add(&object->headers, header->name, header->name_len, header->value, header->value_len);
			
			if (code != KADERR_SUCCESS) {
				break;
			}
		}
	}
	
	if (code != KADERR_SUCCESS) {
		goto end;
	}
	
	/* Body */
	
	headers_size = (size_t) consumed - offset_delta;
	body_size = size - headers_size;
	
	if (body_size > 0) {
		object->body.content = malloc(body_size);
		
		if (object->body.content == NULL) {
			code = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		memcpy(object->body.content, buffer + headers_size, body_size);
		object->body.size = body_size;
	}
	
	end:;
	
	free(patched_buffer);
	
	return code;
	
}

static void http_headers_free(http_headers_t* const headers) {
	
	size_t index = 0;
	
	http_header_t* header = NULL;
	
	if (headers->items == NULL) {
		return;
	}
	
	for (index = 0; index < headers->offset; index++) {
		header = &headers->items[index];
		
		free(header->key);
		header->key = NULL;
		
		header->value = NULL;
	}
	
	free(headers->items);
	headers->items = NULL;
	
	headers->offset = 0;
	headers->capacity = 0;
	
}

static void http_body_free(http_body_t* const body) {
	
	if (body->size < 1) {
		return;
	}
	
	free(body->content);
	
	body->content = NULL;
	body->size = 0;
	
}

void http_request_free(http_request_t* const request) {
	http_object_free((http_object_t*) request);
}

void http_response_free(http_response_t* const response) {
	http_object_free((http_object_t*) response);
}

void http_object_free(http_object_t* const object) {
	
	object->version = (enum HTTPVersion) 0;
	object->method = (http_method_t) 0;
	
	http_headers_free(&object->headers);
	http_body_free(&object->body);
	
	free(object->uri);
	object->uri = NULL;
	
	object->ptr = NULL;
	
}
