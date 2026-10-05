#if !defined(HTTP_H)
#define HTTP_H

#include <stdlib.h>
#include <stdio.h>

enum HTTPMethod {
	GET = 1,
	HEAD = 2,
	POST = 3,
	PUT = 4,
	DELETE = 5,
	CONNECT = 6,
	OPTIONS = 7,
	TRACE = 8
};

enum HTTPVersion {
	HTTP10 = 1,
	HTTP11 = 2,
	HTTP2 = 3,
	HTTP3 = 4
};

enum HTTPStatusCode {
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,
	PROCESSING = 102,
	EARLY_HINTS = 103,
	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,
	MULTI_STATUS = 207,
	ALREADY_REPORTED = 208,
	IM_USED = 226,
	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305,
	TEMPORARY_REDIRECT = 307,
	PERMANENT_REDIRECT = 308,
	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	PROXY_AUTHENTICATION_REQUIRED = 407,
	REQUEST_TIMEOUT = 408,
	CONFLICT = 409,
	GONE = 410,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	REQUEST_ENTITY_TOO_LARGE = 413,
	REQUEST_URI_TOO_LONG = 414,
	UNSUPPORTED_MEDIA_TYPE = 415,
	REQUESTED_RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,
	IM_A_TEAPOT = 418,
	MISDIRECTED_REQUEST = 421,
	UNPROCESSABLE_ENTITY = 422,
	LOCKED = 423,
	FAILED_DEPENDENCY = 424,
	TOO_EARLY = 425,
	UPGRADE_REQUIRED = 426,
	PRECONDITION_REQUIRED = 428,
	TOO_MANY_REQUESTS = 429,
	REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	UNAVAILABLE_FOR_LEGAL_REASONS = 451,
	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	VARIANT_ALSO_NEGOTIATES = 506,
	INSUFFICIENT_STORAGE = 507,
	LOOP_DETECTED = 508,
	NOT_EXTENDED = 510,
	NETWORK_AUTHENTICATION_REQUIRED = 511
};

enum HTTPObjectType {
	HTTP_RESPONSE,
	HTTP_REQUEST
};

typedef enum HTTPObjectType http_object_type_t;

typedef enum HTTPMethod http_method_t;
typedef enum HTTPVersion http_version_t;
typedef enum HTTPStatusCode http_status_code_t;

struct HTTPHeader {
	char* key;
	char* value;
};

typedef struct HTTPHeader http_header_t;

struct HTTPHeaders {
	size_t offset;
	size_t capacity;
	struct HTTPHeader* items;
	size_t slength;
};

typedef struct HTTPHeaders http_headers_t;

struct HTTPBody {
	size_t size;
	char* content;
};

typedef struct HTTPBody http_body_t;

struct HTTPResponse {
	http_object_type_t type;
	http_status_code_t status;
	http_version_t version;
	http_method_t method;
	http_headers_t headers;
	http_body_t body;
	char* uri;
	const char* ptr;
};

typedef struct HTTPResponse http_response_t;

struct HTTPRequest {
	http_object_type_t type;
	http_status_code_t status;
	http_version_t version;
	http_method_t method;
	http_headers_t headers;
	http_body_t body;
	char* uri;
	const char* ptr;
};

typedef struct HTTPRequest http_request_t;

struct HTTPObject {
	http_object_type_t type;
	http_status_code_t status;
	http_version_t version;
	http_method_t method;
	http_headers_t headers;
	http_body_t body;
	char* uri;
	const char* ptr;
};

typedef struct HTTPObject http_object_t;

const char* http_method_stringify(const http_method_t method);
const char* http_version_stringify(const http_version_t version);
const char* http_status_stringify(const http_status_code_t status_code);

int http_headers_add(http_headers_t* const headers, const char* key, const size_t key_size, const char* value, const size_t value_size);
const http_header_t* http_headers_get(const http_headers_t* const headers, const char* key);

int http_request_parse(http_request_t* const request, const char* const buffer, const size_t size);
int http_response_parse(http_response_t* const response, const char* const buffer, const size_t size);
int http_object_parse(http_object_t* const object, const char* const buffer, const size_t size);

void http_request_free(http_request_t* const request);
void http_response_free(http_response_t* const response);
void http_object_free(http_object_t* const object);

void http_request_init(http_request_t* const request);
void http_response_init(http_response_t* const response);

#endif
