#include <stdlib.h>
#include <string.h>

#include <event2/util.h>
#include <picohttpparser.h>

#include "http.h"
#include "errors.h"
#include "constants.h"

#define MAX_HTTP_HEADERS_COUNT (128)
#define MAX_HTTP_METHOD_SIZE (7)

static const char* const HTTP_METHODS[] = {
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"PATCH",
	"DELETE",
	"CONNECT",
	"OPTIONS",
	"TRACE"
};

static const char* const HTTP_VERSIONS[] = {
	"1.0",
	"1.1",
	"2",
	"3"
};

const char* http_method_stringify(const http_method_t method) {
	return HTTP_METHODS[method];
}

const char* http_version_stringify(const http_version_t version) {
	return HTTP_VERSIONS[version];
}

static int http_headers_reserve(http_headers_t* const headers, const size_t count);

int http_headers_add(http_headers_t* const headers, const char* key, const size_t key_size, const char* value, const size_t value_size) {
	
	int status = KADERR_SUCCESS;
	
	http_header_t header = {NULL, NULL};
	
	if (http_headers_reserve(headers, (headers->offset + 1)) != KADERR_SUCCESS) {
		status = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
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
	
	size_t size = 0;
	
	if (headers->capacity >= count) {
		return status;
	}
	
	size = (headers->capacity == 0) ? 8 : (headers->capacity * 2);
	
	if (size < count) {
		size = count;
	}
	
	items = (http_header_t*) realloc(headers->items, (size * sizeof(http_header_t)));
	
	if (items == NULL) {
		status = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	headers->capacity = size;
	headers->items = items;
	
	end:;
	
	return status;
	
}

const http_header_t* http_headers_get(const http_headers_t* const headers, const char* key) {
	
	size_t index = 0;
	
	const http_header_t* header = NULL;
	
	for (index = 0; index < headers->offset; index++) {
		header = &headers->items[index];
		
		if (evutil_ascii_strcasecmp(header->key, key) == 0) {
			return header;
		}
	}
	
	return NULL;
	
}

http_method_t http_method_unstringify(const char* const name) {
	
	size_t index = 0;
	const char* subname = NULL;
	
	for (index = 0; index < sizeof(HTTP_METHODS) / sizeof(*HTTP_METHODS); index++) {
		subname = HTTP_METHODS[index];
		
		if (strcmp(name, subname) != 0) {
			continue;
		}
		
		return (http_method_t) index;
	}
	
	return GET;
	
}

void http_object_init(http_object_t* const object, const http_object_type_t type) {
	memset(object, 0, sizeof(*object));
	object->type = type;
}


int http_object_parse(http_object_t* const object, const char* const buffer, const size_t size) {
	
	int err = KADERR_SUCCESS;
	
	const char* token = NULL;
	size_t token_size = 0;
	
	const char* path = NULL;
	size_t path_size = 0;
	
	const struct phr_header* header = NULL;
	struct phr_header headers[MAX_HTTP_HEADERS_COUNT];

	size_t index = MAX_HTTP_HEADERS_COUNT;
	
	int status_code = 0;
	int http_version = 0;
	
	int status = 0;

	char method[MAX_HTTP_METHOD_SIZE + 1];

	switch (object->type) {
		case HTTP_REQUEST: {
			status = phr_parse_request(buffer, size, &token, &token_size, &path, &path_size, &http_version, headers, &index, 0);
			break;
		}
		case HTTP_RESPONSE: {
			status = phr_parse_response(buffer, size, &http_version, &object->status, &token, &token_size, headers, &index, 0);
			break;
		}
		default: {
			err = KADERR_HTTP_MALFORMED;
			goto end;
		}
	}

	if (status == -2) {
		err = KADERR_HTTP_INCOMPLETE;
		goto end;
	}
	
	if (status < 0) {
		err = KADERR_HTTP_MALFORMED;
		goto end;
	}

	err = http_headers_reserve(&object->headers, index);

	if (code != KADERR_SUCCESS) {
		goto end;
	}

	object->version = http_version;

	if (object->type == HTTP_REQUEST) {
		if (token_size > MAX_HTTP_METHOD_SIZE) {
			err = KADERR_HTTP_MALFORMED;
			goto end;
		}
		
		strncpy(method, token, token_size);
		method[token_size] = '\0';
		
		object->method = http_method_unstringify(method);
		
		object->uri = malloc(path_size + 1);
		
		if (object->uri == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}

		memcpy(object->uri, path, path_size);
		object->uri[path_size] = '\0';
	}

	for (index = 0; index < headers_count; index++) {
		header = &headers[index];

		err = http_headers_add(&object->headers, header->name, header->name_len, header->value, header->value_len);

		if (code != KADERR_SUCCESS) {
			goto end;
		}
	}

	/* Body */

	index = (size_t) status;

	if (size > index) {
		object->body.size = size - index;
		object->body.content = malloc(object->body.size);

		if (object->body.content == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}

		memcpy(object->body.content, buffer + index, object->body.size);
	}

	end:;
	
	return err;
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

void http_object_free(http_object_t* const object) {
	
	object->version = (enum HTTPVersion) 0;
	object->method = (http_method_t) 0;
	
	http_headers_free(&object->headers);
	http_body_free(&object->body);
	
	free(object->uri);
	object->uri = NULL;
	
	object->ptr = NULL;
	
}
