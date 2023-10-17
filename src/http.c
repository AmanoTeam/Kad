#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "http.h"
#include "errors.h"
#include "constants.h"

static int header_name_safe(const char* const s) {
	
	for (size_t index = 0; index < strlen(s); index++) {
		const char ch = s[index];
		
		const int is_safe = (isalnum(ch) || strchr(HEADER_NAME_SAFE_SYMBOLS, ch) != NULL);
		
		if (!is_safe) {
			return is_safe;
		}
	}
	
	return 1;
	
}

static int header_value_safe(const char* const s) {
	
	for (size_t index = 0; index < strlen(s); index++) {
		const char ch = s[index];
		
		const int is_safe = (isalnum(ch) || strchr(HEADER_VALUE_SAFE_SYMBOLS, ch) != NULL);
		
		if (!is_safe) {
			return is_safe;
		}
	}
	
	return 1;
	
}

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
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	strcpy(header.key, key);
	strcpy(header.value, value);
	
	const size_t size = headers->size + sizeof(struct HTTPHeader) * 1;
	struct HTTPHeader* items = (struct HTTPHeader*) realloc(headers->items, size);
	
	if (items == NULL) {
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
		
		if (strcmp(header->key, key) == 0) {
			return header;
		}
	}
	
	return NULL;
	
}

int http_method_parse(struct HTTPObject* const object) {
	
	if (object->type != HTTP_REQUEST) {
		return KADERR_NOT_IMPLEMENTED;
	}
	
	const char* http_method_start = object->ptr;
	const char* http_method_end = strstr(http_method_start, SPACE);
	
	if (http_method_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	const size_t http_method_size = (size_t) (http_method_end - http_method_start);
	
	char http_method[http_method_size + 1];
	memcpy(http_method, http_method_start, http_method_size);
	http_method[http_method_size] = '\0';
	
	if (strcmp(http_method, "GET") == 0) {
		object->method = GET;
	} else if (strcmp(http_method, "HEAD") == 0) {
		object->method = HEAD;
	} else if (strcmp(http_method, "POST") == 0) {
		object->method = POST;
	} else if (strcmp(http_method, "PUT") == 0) {
		object->method = PUT;
	} else if (strcmp(http_method, "DELETE") == 0) {
		object->method = DELETE;
	} else if (strcmp(http_method, "CONNECT") == 0) {
		object->method = CONNECT;
	} else if (strcmp(http_method, "OPTIONS") == 0) {
		object->method = OPTIONS;
	} else if (strcmp(http_method, "TRACE") == 0) {
		object->method = TRACE;
	}
	
	if (object->method == 0) {
		return KADERR_HTTP_UNKNOWN_METHOD;
	}
	
	object->ptr = http_method_end;
	
	return KADERR_SUCCESS;
	
}

int http_uri_parse(struct HTTPObject* const object) {
	
	if (object->type != HTTP_REQUEST) {
		return KADERR_NOT_IMPLEMENTED;
	}
	
	const char* uri_start = object->ptr;
	uri_start += strlen(SPACE);
	const char* uri_end = strstr(uri_start, SPACE);
	
	if (uri_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	const size_t uri_size = (size_t) (uri_end - uri_start);
	
	object->uri = malloc(uri_size + 1);
	
	if (object->uri == NULL) {
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	memcpy(object->uri, uri_start, uri_size);
	object->uri[uri_size] = '\0';
	
	object->ptr = uri_end;
	
	return KADERR_SUCCESS;
	
}

int http_version_parse(struct HTTPObject* const object) {
	
	const char* http_version_start = strstr(object->ptr, SLASH);
	
	if (http_version_start == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	http_version_start += strlen(SLASH);
	
	const char* http_version_end = strstr(http_version_start, object->type == HTTP_REQUEST ? CRLF : SPACE);
	
	if (http_version_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	const size_t http_version_size = (size_t) (http_version_end - http_version_start);
	
	char http_version[http_version_size + 1];
	memcpy(http_version, http_version_start, http_version_size);
	http_version[http_version_size] = '\0';
	
	if (strcmp(http_version, "1.0") == 0) {
		object->version = HTTP10;
	} else if (strcmp(http_version, "1.1") == 0) {
		object->version = HTTP11;
	} else if (strcmp(http_version, "2") == 0) {
		object->version = HTTP2;
	}

	if (object->version == 0) {
		return KADERR_HTTP_UNSUPPORTED_VERSION;
	}
	
	object->ptr = http_version_end;
	
	return KADERR_SUCCESS;
	
}

int http_status_parse(struct HTTPObject* const object) {
	
	if (object->type != HTTP_RESPONSE) {
		return KADERR_NOT_IMPLEMENTED;
	}
	
	const char* http_status_start = strstr(object->ptr, SPACE);
	
	if (http_status_start == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	http_status_start += strlen(SPACE);
	
	const char* http_status_end = strstr(http_status_start, object->version < HTTP2 ? SPACE : CRLF);
	
	if (http_status_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	const size_t http_status_size = (size_t) (http_status_end - http_status_start);
	
	char http_status[http_status_size + 1];
	memcpy(http_status, http_status_start, http_status_size);
	http_status[http_status_size] = '\0';
	
	const long int status_code = strtol(http_status, NULL, 10);
	object->status = (enum HTTPStatusCode) status_code;
	
	object->ptr = strstr(http_status_end, CRLF); // We don't care about the status message
	
	/*
	if (object->ptr == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	*/
	
	return KADERR_SUCCESS;
	
}

int http_headers_parse(struct HTTPObject* const object) {
	
	const char* header_start = object->ptr;
	header_start += strlen(CRLF);
	const char* header_end = strstr(header_start, CRLF);
	
	if (header_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	const char* headers_end = strchr(header_start, '\0');
	
	if (headers_end == NULL) {
		return KADERR_HTTP_MALFORMED_HEADER;
	}
	
	while (1) {
		if (!header_end) {
			header_end = headers_end;
		}
		
		const char* separator = strstr(header_start, HEADER_SEPARATOR);
		
		if (separator == NULL || separator > header_end) {
			return KADERR_HTTP_MALFORMED_HEADER;
		}
		
		const size_t key_size = (size_t) (separator - header_start);
		
		if (key_size < 1) {
			return KADERR_HTTP_MISSING_HEADER_NAME;
		}
		
		char key[key_size + 1];
		memcpy(key, header_start, key_size);
		key[key_size] = '\0';
		
		if (!header_name_safe(key)) {
			return KADERR_HTTP_HEADER_CONTAINS_INVALID_CHARACTER;
		}
		
		separator += strlen(HEADER_SEPARATOR);
		
		const size_t value_size = (size_t) (header_end - separator);
		
		if (value_size < 1) {
			return KADERR_HTTP_MISSING_HEADER_VALUE;
		}
		
		char value[value_size + 1];
		memcpy(value, separator, value_size);
		value[value_size] = '\0';
		
		if (!header_value_safe(value)) {
			return KADERR_HTTP_HEADER_CONTAINS_INVALID_CHARACTER;
		}
		
		const int code = http_headers_add(&object->headers, key, value);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
		
		if (header_end == headers_end) {
			break;
		}
		
		header_start = header_end;
		header_start += strlen(CRLF);
		
		header_end = strstr(header_start, CRLF);
	}
	
	return KADERR_SUCCESS;
	
}

void http_request_init(struct HTTPRequest* const request) {
	request->type = HTTP_REQUEST;
}

void http_response_init(struct HTTPResponse* const response) {
	response->type = HTTP_RESPONSE;
}

int http_request_parse(struct HTTPRequest* const object, const char* const buffer, const size_t size) {
	return http_object_parse((struct HTTPObject*) object, buffer, size);
}

int http_response_parse(struct HTTPResponse* const object, const char* const buffer, const size_t size) {
	return http_object_parse((struct HTTPObject*) object, buffer, size);
}

int http_object_parse(struct HTTPObject* const object, const char* const buffer, const size_t size) {
	
	const char* separator = NULL;
	
	for (size_t index = 0; index < size; index++) {
		if (index > MAX_HTTP_HEADERS_SIZE) {
			return KADERR_HTTP_HEADERS_TOO_BIG;
		}
		
		const char* start = buffer + index;
		
		int value = memcmp(CRLFCRLF, start, strlen(CRLFCRLF));
		
		if (value == 0) {
			separator = start;
			break;
		}
	}
	
	if (separator == NULL) {
		return KADERR_HTTP_MALFORMED_REQUEST;
	}
	
	// Headers
	const size_t headers_size = (size_t) (separator - buffer);
	
	char headers[headers_size + 1];
	memcpy(headers, buffer, headers_size);
	headers[headers_size] = '\0';
	
	object->ptr = headers;
	
	// Body
	const char* body_start = separator;
	body_start += strlen(CRLFCRLF);
	
	const size_t body_size = (size_t) ((buffer + size) - body_start);
	
	if (body_size > 0) {
		object->body.content = malloc(body_size);
		
		if (object->body.content == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		memcpy(object->body.content, body_start, body_size);
		object->body.size = body_size;
	}
	
	int code = 0;
	
	if (object->type == HTTP_REQUEST) {
		// Parse HTTP method
		code = http_method_parse(object);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
		
		// Parse HTTP URI
		code = http_uri_parse(object);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
	}
	
	// Parse HTTP version
	code = http_version_parse(object);
	
	if (code != KADERR_SUCCESS) {
		return code;
	}
	
	if (object->type == HTTP_RESPONSE) {
		// Parse status code
		code = http_status_parse(object);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
	}
	
	if (object->ptr != NULL) {
		// Parse HTTP headers
		code = http_headers_parse(object);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
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
	
}
	