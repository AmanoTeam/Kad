#if !defined(HTTP_H)
#define HTTP_H

#include <stdlib.h>
#include <stdio.h>

#undef DELETE

enum HTTPMethod {
	GET = 0x00,
	HEAD = 0x01,
	POST = 0x02,
	PUT = 0x03,
	PATCH = 0x04,
	DELETE = 0x05,
	CONNECT = 0x06,
	OPTIONS = 0x07,
	TRACE = 0x08
};

enum HTTPVersion {
	HTTP10 = 0x00,
	HTTP11 = 0x01,
	HTTP2 = 0x02,
	HTTP3 = 0x03,
};

enum HTTPObjectType {
	HTTP_RESPONSE,
	HTTP_REQUEST
};

typedef enum HTTPObjectType http_object_type_t;

typedef enum HTTPMethod http_method_t;
typedef enum HTTPVersion http_version_t;

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

struct HTTPObject {
	http_object_type_t type;
	int status;
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

int http_headers_add(http_headers_t* const headers, const char* key, const size_t key_size, const char* value, const size_t value_size);
const http_header_t* http_headers_get(const http_headers_t* const headers, const char* key);

void http_object_init(http_object_t* const object, const http_object_type_t type);
int http_object_parse(http_object_t* const object, const char* const buffer, const size_t size);
void http_object_free(http_object_t* const object);

#endif
