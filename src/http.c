#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <picohttpparser.h>

#include "http.h"
#include "errors.h"
#include "constants.h"

static const size_t MAX_HTTP_HEADERS_COUNT = 128;

const char* http_method_stringify(const enum HTTPMethod method) {
	
	switch (method) {
		case GET:
			return "GET";
		case HEAD:
			return "HEAD";
		case POST:
			return "POST";
		case PUT:
			return "PUT";
		case DELETE:
			return "DELETE";
		case CONNECT:
			return "CONNECT";
		case OPTIONS:
			return "OPTIONS";
		case TRACE:
			return "TRACE";
	}
	
	return NULL;
	
}

const char* http_version_stringify(const enum HTTPVersion version) {
	
	switch (version) {
		case HTTP10:
			return "1.0";
		case HTTP11:
			return "1.1";
		case HTTP2:
			return "2";
	}
	
	return NULL;
	
}

const char* http_status_stringify(const enum HTTPStatusCode status_code) {
	
	switch (status_code) {
		case CONTINUE:
			return "Continue";
		case SWITCHING_PROTOCOLS:
			return "Switching Protocols";
		case PROCESSING:
			return "Processing";
		case EARLY_HINTS:
			return "Early Hints";
		case OK:
			return "OK";
		case CREATED:
			return "Created";
		case ACCEPTED:
			return "Accepted";
		case NON_AUTHORITATIVE_INFORMATION:
			return "Non-Authoritative Information";
		case NO_CONTENT:
			return "No Content";
		case RESET_CONTENT:
			return "Reset Content";
		case PARTIAL_CONTENT:
			return "Partial Content";
		case MULTI_STATUS:
			return "Multi-Status";
		case ALREADY_REPORTED:
			return "Already Reported";
		case IM_USED:
			return "IM Used";
		case MULTIPLE_CHOICES:
			return "Multiple Choices";
		case MOVED_PERMANENTLY:
			return "Moved Permanently";
		case FOUND:
			return "Found";
		case SEE_OTHER:
			return "See Other";
		case NOT_MODIFIED:
			return "Not Modified";
		case USE_PROXY:
			return "Use Proxy";
		case TEMPORARY_REDIRECT:
			return "Temporary Redirect";
		case PERMANENT_REDIRECT:
			return "Permanent Redirect";
		case BAD_REQUEST:
			return "Bad Request";
		case UNAUTHORIZED:
			return "Unauthorized";
		case PAYMENT_REQUIRED:
			return "Payment Required";
		case FORBIDDEN:
			return "Forbidden";
		case NOT_FOUND:
			return "Not Found";
		case METHOD_NOT_ALLOWED:
			return "Method Not Allowed";
		case NOT_ACCEPTABLE:
			return "Not Acceptable";
		case PROXY_AUTHENTICATION_REQUIRED:
			return "Proxy Authentication Required";
		case REQUEST_TIMEOUT:
			return "Request Timeout";
		case CONFLICT:
			return "Conflict";
		case GONE:
			return "Gone";
		case LENGTH_REQUIRED:
			return "Length Required";
		case PRECONDITION_FAILED:
			return "Precondition Failed";
		case REQUEST_ENTITY_TOO_LARGE:
			return "Request Entity Too Large";
		case REQUEST_URI_TOO_LONG:
			return "Request-URI Too Long";
		case UNSUPPORTED_MEDIA_TYPE:
			return "Unsupported Media Type";
		case REQUESTED_RANGE_NOT_SATISFIABLE:
			return "Requested Range Not Satisfiable";
		case EXPECTATION_FAILED:
			return "Expectation Failed";
		case IM_A_TEAPOT:
			return "I'm a Teapot";
		case MISDIRECTED_REQUEST:
			return "Misdirected Request";
		case UNPROCESSABLE_ENTITY:
			return "Unprocessable Entity";
		case LOCKED:
			return "Locked";
		case FAILED_DEPENDENCY:
			return "Failed Dependency";
		case TOO_EARLY:
			return "Too Early";
		case UPGRADE_REQUIRED:
			return "Upgrade Required";
		case PRECONDITION_REQUIRED:
			return "Precondition Required";
		case TOO_MANY_REQUESTS:
			return "Too Many Requests";
		case REQUEST_HEADER_FIELDS_TOO_LARGE:
			return "Request Header Fields Too Large";
		case UNAVAILABLE_FOR_LEGAL_REASONS:
			return "Unavailable For Legal Reasons";
		case INTERNAL_SERVER_ERROR:
			return "Internal Server Error";
		case NOT_IMPLEMENTED:
			return "Not Implemented";
		case BAD_GATEWAY:
			return "Bad Gateway";
		case SERVICE_UNAVAILABLE:
			return "Service Unavailable";
		case GATEWAY_TIMEOUT:
			return "Gateway Timeout";
		case HTTP_VERSION_NOT_SUPPORTED:
			return "HTTP Version Not Supported";
		case VARIANT_ALSO_NEGOTIATES:
			return "Variant Also Negotiates";
		case INSUFFICIENT_STORAGE:
			return "Insufficient Storage";
		case LOOP_DETECTED:
			return "Loop Detected";
		case NOT_EXTENDED:
			return "Not Extended";
		case NETWORK_AUTHENTICATION_REQUIRED:
			return "Network Authentication Required";
	}
	
	return NULL;
	
}

int http_headers_add(struct HTTPHeaders* const headers, const char* key, const char* value) {
	
	struct HTTPHeader header = {
		.key = malloc(strlen(key) + 1),
		.value = malloc(strlen(value) + 1)
	};
	
	if (header.key == NULL || header.value == NULL) {
		free(header.key);
		free(header.value);
		
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(header.key, key);
	strcpy(header.value, value);
	
	const size_t size = headers->size + sizeof(struct HTTPHeader) * 1;
	struct HTTPHeader* items = (struct HTTPHeader*) realloc(headers->items, size);
	
	if (items == NULL) {
		free(header.key);
		free(header.value);
		
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	headers->size = size;
	headers->items = items;
	headers->items[headers->offset++] = header;
	
	if (headers->slength > 0) {
		headers->slength += strlen(CRLF);
	}
	
	if (key != NULL) {
		headers->slength += strlen(key);
	}
	
	headers->slength += strlen(COLON) + strlen(SPACE);
	
	if (value != NULL) {
		headers->slength += strlen(value);
	}
	
	return KADERR_SUCCESS;
	
}

const struct HTTPHeader* http_headers_get(const struct HTTPHeaders* const headers, const char* key) {
	
	for (size_t index = 0; index < headers->offset; index++) {
		const struct HTTPHeader* header = &headers->items[index];
		
		if (strcasecmp(header->key, key) == 0) {
			return header;
		}
	}
	
	return NULL;
	
}

static enum HTTPMethod http_method_from_string(const char* const method, const size_t size) {
	
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
	
	for (size_t index = 0; index < sizeof(methods) / sizeof(methods[0]); index++) {
		if (strlen(methods[index]) == size && memcmp(methods[index], method, size) == 0) {
			return (enum HTTPMethod) (index + 1);
		}
	}
	
	return (enum HTTPMethod) 0;
	
}

static int http_headers_add_slice(struct HTTPHeaders* const headers, const char* const key, const size_t key_size, const char* const value, const size_t value_size) {
	
	char key_str[key_size + 1];
	memcpy(key_str, key, key_size);
	key_str[key_size] = '\0';
	
	char value_str[value_size + 1];
	memcpy(value_str, value, value_size);
	value_str[value_size] = '\0';
	
	return http_headers_add(headers, key_str, value_str);
	
}

void http_request_init(struct HTTPRequest* const request) {
	memset(request, 0, sizeof(*request));
	request->type = HTTP_REQUEST;
}

void http_response_init(struct HTTPResponse* const response) {
	memset(response, 0, sizeof(*response));
	response->type = HTTP_RESPONSE;
}

int http_request_parse(struct HTTPRequest* const object, const char* const buffer, const size_t size) {
	return http_object_parse((struct HTTPObject*) object, buffer, size);
}

int http_response_parse(struct HTTPResponse* const object, const char* const buffer, const size_t size) {
	return http_object_parse((struct HTTPObject*) object, buffer, size);
}

int http_object_parse(struct HTTPObject* const object, const char* const buffer, const size_t size) {
	
	if (object->type == HTTP_REQUEST && size > (size_t) MAX_HTTP_HEADERS_SIZE) {
		return KADERR_HTTP_HEADERS_TOO_BIG;
	}
	
	// cURL reports HTTP/2 responses with an "HTTP/2 <code>" status line, which the parser doesn't understand, so we rewrite it as HTTP/1.1 and adjust for the 2 extra bytes later
	const char* parse_buffer = buffer;
	size_t parse_size = size;
	
	char* patched_buffer = NULL;
	size_t offset_delta = 0;
	int is_http2 = 0;
	
	if (object->type == HTTP_RESPONSE && size > strlen("HTTP/2 ") && memcmp(buffer, "HTTP/2", strlen("HTTP/2")) == 0 && (buffer[strlen("HTTP/2")] == ' ' || buffer[strlen("HTTP/2")] == '\r')) {
		patched_buffer = malloc(size + 2);
		
		if (patched_buffer == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		memcpy(patched_buffer, "HTTP/1.1", strlen("HTTP/1.1"));
		memcpy(patched_buffer + strlen("HTTP/1.1"), buffer + strlen("HTTP/2"), size - strlen("HTTP/2"));
		
		parse_buffer = patched_buffer;
		parse_size = size + 2;
		offset_delta = 2;
		is_http2 = 1;
	}
	
	const char* method = NULL;
	size_t method_size = 0;
	
	const char* path = NULL;
	size_t path_size = 0;
	
	const char* message = NULL;
	size_t message_size = 0;
	
	int status = 0;
	int minor_version = -1;
	
	struct phr_header headers[MAX_HTTP_HEADERS_COUNT];
	size_t headers_count = MAX_HTTP_HEADERS_COUNT;
	
	const int consumed = (
		(object->type == HTTP_REQUEST)
			? phr_parse_request(parse_buffer, parse_size, &method, &method_size, &path, &path_size, &minor_version, headers, &headers_count, 0)
			: phr_parse_response(parse_buffer, parse_size, &minor_version, &status, &message, &message_size, headers, &headers_count, 0)
	);
	
	if (consumed < 0) {
		free(patched_buffer);
		return KADERR_HTTP_MALFORMED_REQUEST;
	}
	
	if (headers_count == MAX_HTTP_HEADERS_COUNT) {
		free(patched_buffer);
		return KADERR_HTTP_HEADERS_TOO_BIG;
	}
	
	int code = KADERR_SUCCESS;
	
	switch (minor_version) {
		case 0:
			object->version = HTTP10;
			break;
		case 1:
			object->version = HTTP11;
			break;
		default:
			code = KADERR_HTTP_UNSUPPORTED_VERSION;
			break;
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
		object->status = (enum HTTPStatusCode) status;
	}
	
	if (code == KADERR_SUCCESS) {
		for (size_t index = 0; index < headers_count; index++) {
			const struct phr_header* const header = &headers[index];
			
			code = http_headers_add_slice(&object->headers, header->name, header->name_len, header->value, header->value_len);
			
			if (code != KADERR_SUCCESS) {
				break;
			}
		}
	}
	
	free(patched_buffer);
	
	if (code != KADERR_SUCCESS) {
		return code;
	}
	
	// Body
	const size_t headers_size = (size_t) consumed - offset_delta;
	const size_t body_size = size - headers_size;
	
	if (body_size > 0) {
		object->body.content = malloc(body_size);
		
		if (object->body.content == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		memcpy(object->body.content, buffer + headers_size, body_size);
		object->body.size = body_size;
	}
	
	return KADERR_SUCCESS;
	
}

static void http_headers_free(struct HTTPHeaders* const headers) {
	
	if (headers->size < 1) {
		return;
	}
	
	for (size_t index = 0; index < headers->offset; index++) {
		struct HTTPHeader* const header = &headers->items[index];
		
		if (header->key != NULL) {
			free(header->key);
			header->key = NULL;
		}
		
		if (header->value != NULL) {
			free(header->value);
			header->value = NULL;
		}
	}
	
	free(headers->items);
	headers->items = NULL;
	
	headers->size = 0;
	headers->offset = 0;
	
}

static void http_body_free(struct HTTPBody* const body) {
	
	if (body->size < 1) {
		return;
	}
	
	free(body->content);
	
	body->content = NULL;
	body->size = 0;
	
}

void http_request_free(struct HTTPRequest* const request) {
	http_object_free((struct HTTPObject*) request);
}

void http_response_free(struct HTTPResponse* const response) {
	http_object_free((struct HTTPObject*) response);
}

void http_object_free(struct HTTPObject* const object) {
	
	object->version = (enum HTTPVersion) 0;
	object->method = (enum HTTPMethod) 0;
	
	http_headers_free(&object->headers);
	http_body_free(&object->body);
	
	free(object->uri);
	object->uri = NULL;
	
	object->ptr = NULL;
	
}
	