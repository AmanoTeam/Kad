#if defined(KAD_CAPTURE)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#if defined(_WIN32)
	#include <direct.h>
#endif

#if !defined(_WIN32)
	#include <sys/stat.h>
	#include <sys/types.h>
#endif

#include <curl/curl.h>
#include <jansson.h>

#include "capture.h"
#include "buffer.h"
#include "constants.h"
#include "errors.h"
#include "getexec.h"
#include "logging.h"
#include "sep.h"
#include "systemerror.h"

#define CAPTURE_DIRECTORY_NAME "capture"

#define CAPTURE_REQUEST_JSON_FILENAME "request.json"
#define CAPTURE_REQUEST_BODY_FILENAME "request_body.bin"
#define CAPTURE_RESPONSE_JSON_FILENAME "response.json"
#define CAPTURE_RESPONSE_BODY_FILENAME "response_body.bin"

#define CAPTURE_MAX_DIRECTORY_ATTEMPTS (1000)

static const char CAPTURE_CONNECT_PREFIX[] = "CONNECT ";

struct capture_header {
	char* key;
	char* value;
};

typedef struct capture_header capture_header_t;

static int capture_header_compare(const void* const left, const void* const right) {

	const capture_header_t* const first = (const capture_header_t*) left;
	const capture_header_t* const second = (const capture_header_t*) right;

	return strcmp(first->key, second->key);

}

static char* capture_strdup(const char* const string) {

	char* copy = malloc(strlen(string) + 1);

	if (copy == NULL) {
		return NULL;
	}

	strcpy(copy, string);

	return copy;

}

static int capture_mkdir(const char* const path) {
	/*
	Creates a directory.

	Returns (0) on success, (1) if it already exists and (-1) on error.
	*/

	#if defined(_WIN32)
		if (_mkdir(path) == 0) {
			return 0;
		}
	#else
		if (mkdir(path, 0777) == 0) {
			return 0;
		}
	#endif

	if (errno == EEXIST) {
		return 1;
	}

	return -1;

}

static char* capture_directory_create(const time_t timestamp) {
	/*
	Creates the directory that will hold the captured files.

	Returns the path of the newly created directory, or a null pointer on error.
	*/

	system_error_t error = {0};

	char* app_directory = NULL;
	char* root = NULL;
	char* directory = NULL;

	char name[64];

	int status = 0;
	int attempt = 1;

	app_directory = get_app_directory();

	if (app_directory == NULL) {
		loggln(LOG_ERROR, "[error] could not get application directory: %s", strkaderr(KADERR_FS_GET_APP_DIRECTORY_FAILURE));

		goto end;
	}

	root = malloc(strlen(app_directory) + strlen(PATHSEP_S) + strlen(CAPTURE_DIRECTORY_NAME) + 1);

	if (root == NULL) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);

		goto end;
	}

	strcpy(root, app_directory);
	strcat(root, PATHSEP_S);
	strcat(root, CAPTURE_DIRECTORY_NAME);

	status = capture_mkdir(root);

	if (status == -1) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not create directory at '%s': %s", root, error.message);

		goto end;
	}

	while (1) {
		if (attempt == 1) {
			snprintf(name, sizeof(name), "%lld", (long long) timestamp);
		} else {
			snprintf(name, sizeof(name), "%lld_%i", (long long) timestamp, attempt);
		}

		directory = malloc(strlen(root) + strlen(PATHSEP_S) + strlen(name) + 1);

		if (directory == NULL) {
			error = get_system_error();
			loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);

			goto end;
		}

		strcpy(directory, root);
		strcat(directory, PATHSEP_S);
		strcat(directory, name);

		status = capture_mkdir(directory);

		if (status == 0) {
			break;
		}

		free(directory);
		directory = NULL;

		if (status == -1 || attempt >= CAPTURE_MAX_DIRECTORY_ATTEMPTS) {
			error = get_system_error();
			loggln(LOG_ERROR, "[error] could not create directory at '%s': %s", root, error.message);

			goto end;
		}

		attempt++;
	}

	end:;
	free(app_directory);
	free(root);

	return directory;

}

static fstream_t* capture_file_open(const char* const directory, const char* const filename) {

	fstream_t* stream = NULL;

	char* path = malloc(strlen(directory) + strlen(PATHSEP_S) + strlen(filename) + 1);

	if (path == NULL) {
		return NULL;
	}

	strcpy(path, directory);
	strcat(path, PATHSEP_S);
	strcat(path, filename);

	stream = fstream_open(path, FSTREAM_WRITE);

	if (stream == NULL) {
		system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not open file at '%s': %s", path, error.message);
	}

	free(path);

	return stream;

}

static int capture_file_write(const char* const directory, const char* const filename, const char* const buffer, const size_t size) {

	fstream_t* stream = NULL;

	char* path = malloc(strlen(directory) + strlen(PATHSEP_S) + strlen(filename) + 1);

	int err = KADERR_SUCCESS;

	if (path == NULL) {
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}

	strcpy(path, directory);
	strcat(path, PATHSEP_S);
	strcat(path, filename);

	stream = fstream_open(path, FSTREAM_WRITE);

	if (stream == NULL) {
		system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not open file at '%s': %s", path, error.message);

		err = KADERR_FSTREAM_OPEN_FAILURE;
		goto end;
	}

	if (fstream_write(stream, buffer, size) != FSTREAM_SUCCESS) {
		system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not write to file at '%s': %s", path, error.message);

		err = KADERR_FSTREAM_WRITE_FAILURE;
	}

	fstream_close(stream);
	stream = NULL;

	end:;
	free(path);

	return err;

}

static json_t* capture_json_string(const char* const string) {
	/*
	Creates a JSON string, replacing non-ASCII bytes with a question mark
	if the string is not valid UTF-8.
	*/

	json_t* value = json_string(string);

	if (value != NULL) {
		return value;
	}

	{
		size_t index = 0;
		size_t size = strlen(string);

		char* sanitized = malloc(size + 1);

		if (sanitized == NULL) {
			return NULL;
		}

		for (index = 0; index < size; index++) {
			sanitized[index] = ((((const unsigned char*) string)[index] < 0x80) ? string[index] : '?');
		}

		sanitized[size] = '\0';

		value = json_string(sanitized);

		free(sanitized);
	}

	return value;

}

static int capture_json_set_string(json_t* const object, const char* const key, const char* const string) {

	json_t* value = capture_json_string(string);

	if (value == NULL) {
		return -1;
	}

	if (json_object_set_new(object, key, value) != 0) {
		json_decref(value);
		return -1;
	}

	return 0;

}

static json_t* capture_headers_json(const char* const buffer, const size_t size, const int lowercase) {
	/*
	Converts a raw HTTP header block into a JSON object whose keys are
	sorted alphabetically. Repeated headers are merged with a comma.
	*/

	json_t* object = NULL;

	capture_header_t* items = NULL;
	size_t offset = 0;
	size_t capacity = 0;

	const char* position = buffer;
	const char* end = (buffer + size);

	const char* line = NULL;
	const char* line_end = NULL;
	const char* separator = NULL;
	const char* value = NULL;

	size_t key_size = 0;
	size_t value_size = 0;

	size_t index = 0;
	size_t last = 0;

	int firstline = 1;

	while (position < end) {
		line = position;

		line_end = memchr(line, '\n', (size_t) (end - line));

		if (line_end == NULL) {
			line_end = end;
		}

		position = line_end + 1;

		/* skip the request/status line, it is not a header */

		if (firstline) {
			firstline = 0;
			continue;
		}

		if (line_end > line && *(line_end - 1) == '\r') {
			line_end--;
		}

		separator = memchr(line, ':', (size_t) (line_end - line));

		/* skip the request/status line, empty lines, obs-fold continuations and pseudo-headers */

		if (separator == NULL || separator == line || *line == ' ' || *line == '\t' || *line == ':') {
			continue;
		}

		key_size = (size_t) (separator - line);

		value = (separator + 1);

		if (value < line_end && *value == ' ') {
			value++;
		}

		value_size = (size_t) (line_end - value);

		if (offset >= capacity) {
			capture_header_t* tmp = NULL;

			capacity = ((capacity == 0) ? 8 : (capacity * 2));

			tmp = realloc(items, capacity * sizeof(*items));

			if (tmp == NULL) {
				goto failure;
			}

			items = tmp;
		}

		items[offset].key = malloc(key_size + 1);
		items[offset].value = malloc(value_size + 1);

		if (items[offset].key == NULL || items[offset].value == NULL) {
			free(items[offset].key);
			free(items[offset].value);

			goto failure;
		}

		memcpy(items[offset].key, line, key_size);
		items[offset].key[key_size] = '\0';

		if (lowercase) {
			char* key = items[offset].key;

			while (*key != '\0') {
				*key = (char) tolower((unsigned char) *key);
				key++;
			}
		}

		memcpy(items[offset].value, value, value_size);
		items[offset].value[value_size] = '\0';

		offset++;
	}

	qsort(items, offset, sizeof(*items), capture_header_compare);

	object = json_object();

	if (object == NULL) {
		goto failure;
	}

	/* merge repeated headers with a comma; after sorting they are adjacent */

	for (index = 1; index < offset; index++) {
		if (strcmp(items[index].key, items[last].key) == 0) {
			char* merged = malloc(strlen(items[last].value) + strlen(", ") + strlen(items[index].value) + 1);

			if (merged == NULL) {
				goto failure;
			}

			strcpy(merged, items[last].value);
			strcat(merged, ", ");
			strcat(merged, items[index].value);

			free(items[last].value);
			items[last].value = merged;

			free(items[index].key);
			items[index].key = NULL;

			free(items[index].value);
			items[index].value = NULL;
		} else {
			last = index;
		}
	}

	for (index = 0; index < offset; index++) {
		if (items[index].key == NULL) {
			continue;
		}

		if (capture_json_set_string(object, items[index].key, items[index].value) != 0) {
			goto failure;
		}
	}

	for (index = 0; index < offset; index++) {
		free(items[index].key);
		free(items[index].value);
	}

	free(items);

	return object;

	failure:;
	for (index = 0; index < offset; index++) {
		free(items[index].key);
		free(items[index].value);
	}

	free(items);

	json_decref(object);

	return NULL;

}

static const char* capture_protocol_stringify(const long version) {

	switch (version) {
		case CURL_HTTP_VERSION_2_0:
		case CURL_HTTP_VERSION_2TLS:
		case CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE: {
			return "h2";
		}
		case CURL_HTTP_VERSION_3:
		case CURL_HTTP_VERSION_3ONLY: {
			return "h3";
		}
		case CURL_HTTP_VERSION_1_0: {
			return "http/1.0";
		}
	}

	return "http/1.1";

}

static void capture_write_json(capture_t* const capture, const char* const filename, json_t* const root) {

	char* dump = json_dumps(root, JSON_INDENT(2));

	if (dump == NULL) {
		loggln(LOG_ERROR, "[error] could not serialize '%s': %s", filename, strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));
		return;
	}

	capture_file_write(capture->directory, filename, dump, strlen(dump));

	free(dump);

}

static void capture_write_request(capture_t* const capture, const long version) {

	json_t* root = NULL;
	json_t* headers = NULL;

	const int lowercase = (
		version == CURL_HTTP_VERSION_2_0 ||
		version == CURL_HTTP_VERSION_2TLS ||
		version == CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE ||
		version == CURL_HTTP_VERSION_3 ||
		version == CURL_HTTP_VERSION_3ONLY
	);

	root = json_object();

	if (root == NULL) {
		goto failure;
	}

	headers = capture_headers_json(capture->request_headers.s, capture->request_headers.slength, lowercase);

	if (headers == NULL) {
		goto failure;
	}

	if (json_object_set_new(root, "headers", headers) != 0) {
		json_decref(headers);
		goto failure;
	}

	if (capture_json_set_string(root, "method", capture->method) != 0) {
		goto failure;
	}

	if (json_object_set_new(root, "time", json_integer((json_int_t) capture->time)) != 0) {
		goto failure;
	}

	if (capture_json_set_string(root, "url", capture->url) != 0) {
		goto failure;
	}

	capture_write_json(capture, CAPTURE_REQUEST_JSON_FILENAME, root);

	json_decref(root);

	return;

	failure:;
	json_decref(root);

}

static void capture_write_response(capture_t* const capture, const long version, const char* const remote_ip, const long remote_port, const char* const url) {

	json_t* root = NULL;
	json_t* headers = NULL;

	root = json_object();

	if (root == NULL) {
		goto failure;
	}

	if (json_object_set_new(root, "code", json_integer((json_int_t) capture->status)) != 0) {
		goto failure;
	}

	headers = capture_headers_json(capture->response_headers, capture->response_headers_size, 0);

	if (headers == NULL) {
		goto failure;
	}

	if (json_object_set_new(root, "headers", headers) != 0) {
		json_decref(headers);
		goto failure;
	}

	if (capture_json_set_string(root, "protocol", capture_protocol_stringify(version)) != 0) {
		goto failure;
	}

	if (capture_json_set_string(root, "remoteIp", ((remote_ip != NULL) ? remote_ip : "")) != 0) {
		goto failure;
	}

	if (json_object_set_new(root, "remotePort", json_integer((json_int_t) remote_port)) != 0) {
		goto failure;
	}

	if (json_object_set_new(root, "time", json_integer((json_int_t) capture->response_time)) != 0) {
		goto failure;
	}

	if (capture_json_set_string(root, "url", ((url != NULL) ? url : capture->url)) != 0) {
		goto failure;
	}

	capture_write_json(capture, CAPTURE_RESPONSE_JSON_FILENAME, root);

	json_decref(root);

	return;

	failure:;
	json_decref(root);

}

void capture_init(capture_t* const capture, const http_request_t* const request) {
	/*
	Prepares a directory to store the captured request/response.

	Failures disable the capture but never affect the request being forwarded.
	*/

	memset(capture, 0, sizeof(*capture));

	capture->method = http_method_stringify(request->method);
	capture->time = time(NULL);

	capture->url = capture_strdup(request->uri);

	if (capture->url == NULL) {
		loggln(LOG_ERROR, "[error] could not allocate memory: %s", strkaderr(KADERR_MEMORY_ALLOCATE_FAILURE));
		return;
	}

	capture->directory = capture_directory_create(capture->time);

	if (capture->directory == NULL) {
		capture_close(capture);
		return;
	}

	capture->request_body = capture_file_open(capture->directory, CAPTURE_REQUEST_BODY_FILENAME);
	capture->response_body = capture_file_open(capture->directory, CAPTURE_RESPONSE_BODY_FILENAME);

	if (capture->request_body == NULL || capture->response_body == NULL) {
		capture_close(capture);
		return;
	}

}

void capture_request_headers(capture_t* const capture, const char* const buffer, const size_t size) {

	char* s = NULL;

	if (capture->directory == NULL || capture->request_headers_done) {
		return;
	}

	s = realloc(capture->request_headers.s, capture->request_headers.slength + size);

	if (s == NULL) {
		return;
	}

	memcpy(s + capture->request_headers.slength, buffer, size);

	capture->request_headers.s = s;
	capture->request_headers.slength += size;

	if (capture->request_headers.slength > strlen(CRLFCRLF) && memcmp(capture->request_headers.s + (capture->request_headers.slength - strlen(CRLFCRLF)), CRLFCRLF, strlen(CRLFCRLF)) == 0) {
		if (strncasecmp(capture->request_headers.s, CAPTURE_CONNECT_PREFIX, strlen(CAPTURE_CONNECT_PREFIX)) == 0) {
			/* the request to establish the proxy tunnel, the actual request follows it */
			buffer_free(&capture->request_headers);
		} else {
			capture->request_headers_done = 1;
		}
	}

}

void capture_request_body(capture_t* const capture, const char* const buffer, const size_t size) {

	if (capture->directory == NULL || capture->request_body == NULL || size == 0) {
		return;
	}

	fstream_write(capture->request_body, buffer, size);

}

void capture_response_body(capture_t* const capture, const char* const buffer, const size_t size) {

	if (capture->directory == NULL || capture->response_body == NULL || size == 0) {
		return;
	}

	fstream_write(capture->response_body, buffer, size);

}

void capture_response(capture_t* const capture, const http_status_code_t status, const char* const headers, const size_t size) {

	char* s = NULL;

	if (capture->directory == NULL) {
		return;
	}

	s = malloc(size);

	if (s == NULL) {
		return;
	}

	memcpy(s, headers, size);

	free(capture->response_headers);

	capture->response_headers = s;
	capture->response_headers_size = size;
	capture->status = status;
	capture->response_time = time(NULL);
	capture->has_response = 1;

}

void capture_end(capture_t* const capture, CURL* const curl) {
	/*
	Writes the request/response JSON files once the transfer is done.
	*/

	long version = CURL_HTTP_VERSION_NONE;
	long remote_port = 0;

	char* remote_ip = NULL;
	char* url = NULL;

	if (capture->directory == NULL) {
		return;
	}

	if (curl != NULL) {
		if (curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &version) != CURLE_OK) {
			version = CURL_HTTP_VERSION_NONE;
		}

		if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_IP, &remote_ip) != CURLE_OK) {
			remote_ip = NULL;
		}

		if (curl_easy_getinfo(curl, CURLINFO_PRIMARY_PORT, &remote_port) != CURLE_OK) {
			remote_port = 0;
		}

		if (curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &url) != CURLE_OK) {
			url = NULL;
		}
	}

	fstream_close(capture->request_body);
	capture->request_body = NULL;

	fstream_close(capture->response_body);
	capture->response_body = NULL;

	if (capture->request_headers.slength > 0) {
		capture_write_request(capture, version);
	}

	if (capture->has_response) {
		capture_write_response(capture, version, remote_ip, remote_port, url);
	}

}

void capture_close(capture_t* const capture) {

	fstream_close(capture->request_body);
	fstream_close(capture->response_body);

	buffer_free(&capture->request_headers);

	free(capture->directory);
	free(capture->url);
	free(capture->response_headers);

	memset(capture, 0, sizeof(*capture));

}
#endif
