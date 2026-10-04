#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <curl/curl.h>

#include "http.h"
#include "ssl.h"
#include "cleanup.h"
#include "constants.h"
#include "callbacks.h"
#include "transferdata.h"
#include "errors.h"
#include "systemerror.h"
#include "argparse.h"
#include "kad.h"
#include "threads.h"
#include "logging.h"

#if !defined(KAD_DISABLE_SSL_VERIFY)
	#include "fstream.h"
	#include "getexec.h"
	#include "sep.h"
#endif

#define KAD_DEFAULT_IMPERSONATE_TARGET "chrome116"

static const char KAD_DEFAULT_LISTEN_ADDRESS[] = "127.0.0.1";

#define KAD_DEFAULT_LISTEN_PORT (4000)
#define KAD_DEFAULT_LISTEN_BACKLOG (100)

static const char* const IMPERSONATE_HEADERS[] = {
	"Connection",
	"Upgrade",
	"HTTP2-Settings",
	"Sec-CH-UA",
	"Sec-CH-UA-Mobile",
	"Sec-CH-UA-Platform",
	"Upgrade-Insecure-Requests",
	"Accept",
	"User-Agent",
	"Sec-Fetch-Site",
	"Sec-Fetch-Mode",
	"Sec-Fetch-User",
	"Sec-Fetch-Dest",
	"Accept-Encoding",
	"Accept-Language"
};

#if !defined(KAD_DISABLE_SSL_VERIFY)
	static const char CA_CERT_FILENAME[] =
		PATHSEP_M
		"etc"
		PATHSEP_M
		"tls"
		PATHSEP_M
		"cert.pem";
	
	static struct curl_blob curl_blob_global = {0};
#endif

static int fd = 0;

static CURLM* curl_multi = NULL;

#if !defined(KAD_BUILD_SHARED)
static void sigint_handler(void) {
	
	close(fd);
	exit(EXIT_SUCCESS);
	
}
#endif

static char target_impersonate[64] = {0};

static char proxy_url[4096] = {0};

static char doh_url[4096] = {0};

#if !defined(KAD_DISABLE_SSL_VERIFY)
static int load_ssl_certificates(void) {
	/*
	Loads the CA certificate bundle into memory for cURL to use.
	
	Returns (0) on success, (-1) on error.
	*/
	
	int err = KADERR_SUCCESS;
	
	char* app_directory = NULL;
	char* ca_bundle = NULL;
	
	fstream_t* stream = NULL;
	
	int64_t file_size = 0;
	ssize_t size = 0;
	
	system_error_t error = {0};
	
	app_directory = get_app_directory();
	
	if (app_directory == NULL) {
		loggln(LOG_ERROR, "[error] could not get application directory: %s", strkaderr(KADERR_FS_GET_APP_DIRECTORY_FAILURE));
		
		err = KADERR_FS_GET_APP_DIRECTORY_FAILURE;
		goto end;
	}
	
	ca_bundle = malloc(strlen(app_directory) + strlen(CA_CERT_FILENAME) + 1);
	
	if (ca_bundle == NULL) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);
		
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	strcpy(ca_bundle, app_directory);
	strcat(ca_bundle, CA_CERT_FILENAME);
	
	free(app_directory);
	app_directory = NULL;
	
	stream = fstream_open(ca_bundle, FSTREAM_READ);
	
	if (stream == NULL) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not open file at '%s': %s", ca_bundle, error.message);
		
		err = KADERR_FSTREAM_OPEN_FAILURE;
		goto end;
	}
	
	file_size = fstream_size(stream);
	
	if (file_size == FSTREAM_ERROR) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not get file size of '%s': %s", ca_bundle, error.message);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_FSTREAM_TELL_FAILURE;
		goto end;
	}
	
	if (file_size == 0) {
		loggln(LOG_ERROR, "[error] file at '%s' is empty", ca_bundle);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_FSTREAM_READ_EMPTY_FILE;
		goto end;
	}
	
	curl_blob_global.data = malloc((size_t) file_size);
	
	if (curl_blob_global.data == NULL) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	size = fstream_read(stream, curl_blob_global.data, (size_t) file_size);
	
	if (size == FSTREAM_ERROR) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not read contents of file at '%s': %s", ca_bundle, error.message);
		
		fstream_close(stream);
		stream = NULL;
		
		err = KADERR_FSTREAM_READ_FAILURE;
		goto end;
	}
	
	if (fstream_close(stream) == FSTREAM_ERROR) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not close file at '%s': %s", ca_bundle, error.message);
		
		stream = NULL;
		
		err = KADERR_FSTREAM_CLOSE_FAILURE;
		goto end;
	}
	
	stream = NULL;
	
	curl_blob_global.len = (size_t) size;
	
	end:;
	
	if (stream != NULL) {
		fstream_close(stream);
	}
	
	free(app_directory);
	free(ca_bundle);
	
	return err;
	
}
#endif

struct curl_pending {
	CURL* handle;
	struct curl_pending* next;
};

typedef struct curl_pending curl_pending_t;

static curl_pending_t* curl_pending = NULL;
static pthread_mutex_t curl_pending_mutex = PTHREAD_MUTEX_INITIALIZER;

static void* event_loop(void* pointer) {
	
	int left = 0;
	int running = 0;
	
	int err = 0;
	
	CURLMsg* msg = NULL;
	CURLcode easy_code = 0;
	
	CURLMcode code = CURLM_OK;
	
	curl_pending_t* item = NULL;
	curl_pending_t* next = NULL;
	
	CURL* handle = NULL;
	
	transferdata_t* data = NULL;
	
	(void) pointer;
	
	while (1) {
		pthread_mutex_lock(&curl_pending_mutex);
		item = curl_pending;
		curl_pending = NULL;
		pthread_mutex_unlock(&curl_pending_mutex);
		
		while (item != NULL) {
			next = item->next;
			
			handle = item->handle;
			free(item);
			
			code = curl_multi_add_handle(curl_multi, handle);
			
			if (code != CURLM_OK) {
				loggln(LOG_ERROR, "[error] could not add handle to multi stack: %s", curl_multi_strerror(code));
				
				easy_code = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &data);
				
				if (easy_code != CURLE_OK) {
					err = KADERR_CURL_GETINFO_FAILURE;
					goto end;
				}
				
				curl_easy_cleanup(handle);
				transferdata_close(data);
			}
			
			item = next;
		}
		
		code = curl_multi_perform(curl_multi, &running);
		
		if (code != CURLM_OK) {
			err = KADERR_CURL_MULTI_PERFORM_FAILURE;
			goto end;
		}
		
		while ((msg = curl_multi_info_read(curl_multi, &left)) != NULL) {
			if (msg->msg != CURLMSG_DONE) {
				continue;
			}
			
			if (msg->data.result != CURLE_OK) {
				loggln(LOG_ERROR, "[error] curl: %s", curl_easy_strerror(msg->data.result));
			}
			
			easy_code = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &data);
			
			if (easy_code != CURLE_OK) {
				err = KADERR_CURL_GETINFO_FAILURE;
				goto end;
			}
			
			code = curl_multi_remove_handle(curl_multi, msg->easy_handle);
			
			if (code != CURLM_OK) {
				err = KADERR_CURL_MULTI_REMOVE_FAILURE;
				goto end;
			}
			
			curl_easy_cleanup(msg->easy_handle);
			transferdata_close(data);
		}
		
		code = curl_multi_poll(curl_multi, NULL, 0, 1000, NULL);
		
		if (code != CURLM_OK) {
			err = KADERR_CURL_MULTI_POLL_FAILURE;
			goto end;
		}
	}
	
	end:;
	
	if (easy_code != CURLE_OK) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(err), curl_easy_strerror(easy_code));
	} else {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(err), curl_multi_strerror(code));
	}
	
	return NULL;
	
}

static int request_handler(void* pointer) {
	
	int err = 0;
	
	int fd = *(int*) pointer;
	
	transferdata_t* data __transferdata_close__ = NULL;
	
	const http_header_t* header = NULL;
	
	char* hostname = NULL;
	char* uri = NULL;
	char* item = NULL;
	
	char buffer[MAX_HTTP_HEADERS_SIZE];
	
	CURL* curl __curl_easy_cleanup__ = NULL;
	
	struct curl_slist* tmp = NULL;
	
	curl_pending_t* pending = NULL;
	
	size_t index = 0;
	size_t subindex = 0;
	
	size_t content_length = 0;
	
	int status = 0;
	
	int is_secure = 0;
	
	ssize_t recv_size = 0;
	ssize_t size = 0;
	
	CURLcode value = 0;
	
	const char* name = NULL;
	
	static char curl_error_message[CURL_ERROR_SIZE] = {0};
	
	free(pointer);
	
	data = malloc(sizeof(*data));
	
	if (data == NULL) {
		close(fd);
		
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	memset(data, 0, sizeof(*data));
	data->fd = fd;
	
	recv_size = recv(fd, buffer, MAX_HTTP_HEADERS_SIZE, 0);
	
	if (recv_size <= 0) {
		err = KADERR_SOCKET_RECV_FAILURE;
		goto end;
	}
	
	http_request_init(&data->request);
	
	err = http_request_parse(&data->request, buffer, (size_t) recv_size);
	
	if (err != KADERR_SUCCESS) {
		goto end;
	}
	
	is_secure = (data->request.method == CONNECT);
	
	data->is_secure = is_secure;

	if (is_secure) {
		size = send(fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
		
		if (size == -1) {
			err = KADERR_SOCKET_SEND_FAILURE;
			goto end;
		}
		
		if (ssl_init(&data->context, &fd) == -1) {
			err = KADERR_SSL_INIT_FAILURE;
			goto end;
		}
		
		size = ssl_recv(&data->context, buffer, sizeof(buffer));
		
		if (size <= 0) {
			err = KADERR_SSL_RECV_FAILURE;
			goto end;
		}
		
		hostname = data->request.uri;
		
		data->request.uri = NULL;
		http_request_free(&data->request);
		
		err = http_request_parse(&data->request, buffer, (size_t) size);
		
		if (err != KADERR_SUCCESS) {
			goto end;
		}
		
		uri = malloc(strlen(HTTPS_SCHEME) + strlen(SCHEME_SEPARATOR) + strlen(hostname) + strlen(data->request.uri) + 1);
		
		if (uri == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		strcpy(uri, HTTPS_SCHEME);
		strcat(uri, SCHEME_SEPARATOR);
		strcat(uri, hostname);
		strcat(uri, data->request.uri);
		
		free(hostname);
		hostname = NULL;
		
		data->request.uri = uri;
		uri = NULL;
	}
	
	loggln(LOG_INFO, "[info] client request to %s", data->request.uri);
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	curl = curl_easy_init();
	
	if (curl == NULL) {
		err = KADERR_CURL_INIT_FAILURE;
		goto end;
	}
	
	if (curl_easy_impersonate(curl, target_impersonate, 1) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (*proxy_url != '\0' && curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (*doh_url != '\0' && curl_easy_setopt(curl, CURLOPT_DOH_URL, doh_url) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	#if defined(__ANDROID__)
		if (curl_easy_setopt(curl, CURLOPT_DNS_SERVERS, "8.8.8.8,8.8.4.4") != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#endif
	
	if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback_empty) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	#if defined(KAD_DISABLE_SSL_VERIFY)
		value = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYPEER, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}

		value = curl_easy_setopt(curl, CURLOPT_DOH_SSL_VERIFYHOST, 0L);

		if (value != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#else
		if (curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &curl_blob_global) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	#endif
	
	if (curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_message) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}

	if (loglevel_get() == LOG_VERBOSE) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	}


	/* Advertise all compression algorithms supported by curl and let it transparently decode the response body */
	if (curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "") != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	for (index = 0; index < data->request.headers.offset; index++) {
		header = &data->request.headers.items[index];
		
		status = 0;
		
		for (subindex = 0; subindex < sizeof(IMPERSONATE_HEADERS) / sizeof(*IMPERSONATE_HEADERS); subindex++) {
			name = IMPERSONATE_HEADERS[subindex];
			
			status = (strcasecmp(header->key, name) == 0);
			
			if (!status) {
				continue;
			}
			
			break;
		}
		
		if (status) {
			continue;
		}
		
		item = malloc(strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + 1);
		
		if (item == NULL) {
			err = KADERR_MEMORY_ALLOCATE_FAILURE;
			goto end;
		}
		
		strcpy(item, header->key);
		strcat(item, HEADER_SEPARATOR);
		strcat(item, header->value);
		
		tmp = curl_slist_append(data->headers, item);
		
		free(item);
		item = NULL;
		
		if (tmp == NULL) {
			err = KADERR_CURL_SLIST_FAILURE;
			goto end;
		}
		
		data->headers = tmp;
	}
	
	if (data->request.method == POST || data->request.method == PUT) {
		header = http_headers_get(&data->request.headers, "Expect");
		
		if (header == NULL) {
			tmp = curl_slist_append(data->headers, "Expect:");
			
			if (tmp == NULL) {
				err = KADERR_CURL_SLIST_FAILURE;
				goto end;
			}
			
			data->headers = tmp;
		}
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data->headers) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_URL, data->request.uri) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (data->request.method != HEAD) {
		if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) data) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
	}
	
	header = http_headers_get(&data->request.headers, "Transfer-Encoding");
	
	status = (header != NULL && strcmp(header->value, "chunked") == 0);
	
	if (!status) {
		header = http_headers_get(&data->request.headers, "Content-Length");
		
		if (header != NULL) {
			content_length = strtoull(header->value, NULL, 10);
		}
	}
	
	if (data->request.body.size > 0 || content_length > 0 || status || recv_size >= MAX_HTTP_HEADERS_SIZE) {
		if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_READDATA, (void*) data) != CURLE_OK) {
			err = KADERR_CURL_SETOPT_FAILURE;
			goto end;
		}
		
		if (content_length > 0) {
			if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) content_length) != CURLE_OK) {
				err = KADERR_CURL_SETOPT_FAILURE;
				goto end;
			}
		}
		
		data->remaining = content_length;
	}
	
	switch (data->request.method) {
		case GET: {
			value = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
			break;
		}
		case POST: {
			value = curl_easy_setopt(curl, CURLOPT_POST, 1L);
			break;
		}
		case HEAD: {
			value = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			break;
		}
		default: {
			name = http_method_stringify(data->request.method);
			value = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, name);
			break;
		}
	}
	
	if (value != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*) data) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_PRIVATE, (void*) data) != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	pending = malloc(sizeof(*pending));
	
	if (pending == NULL) {
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	pending->handle = curl;
	
	pthread_mutex_lock(&curl_pending_mutex);
	
	pending->next = curl_pending;
	curl_pending = pending;
	pending = NULL;
	
	pthread_mutex_unlock(&curl_pending_mutex);
	
	curl_multi_wakeup(curl_multi);
	
	curl = NULL;
	data = NULL;
	
	end:;
	
	free(hostname);
	free(uri);
	free(item);
	free(pending);
	
	return err;
	
}

static void* handle_request(void* pointer) {
	
	const int code = request_handler(pointer);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
	}
	
	return NULL;
	
}

int kad_main(int argc, char* argv[]) {
	/*
	const struct sigaction action = {
		.sa_handler = &sigint_handler
	};
	
	if (sigaction(SIGINT, &action, NULL) < 0) {
		abort_with_perror("sigaction()");
	}
	*/
	
	struct sigaction sigpipe_action;
	
	struct linger lingerv;
	
	struct sockaddr* sockaddress = NULL;
	
	struct sockaddr_in addr_in;
	struct sockaddr_in6 addr_in6;
	
	struct sockaddr_storage client_address;
	
	socklen_t client_address_size = 0;
	
	size_t addrsize = 0;
	
	char address[512] = {0};
	char loglevel[16] = "verbose";
	
	char host[NI_MAXHOST];
	char servname[NI_MAXSERV];
	
	const int reuseaddr = 1;
	
	int port = 0;
	int value = 0;
	
	int position = 0;
	int index = 0;
	
	int code = 0;
	int status = 0;
	int err = 0;
	
	size_t size = 0;
	
	argparse_t argparse = {0};
	const arg_t* argument = NULL;
	
	thread_t event_thread = {0};
	thread_t client_thread = {0};
	thread_t threads[KAD_DEFAULT_LISTEN_BACKLOG];
	
	thread_t* worker = NULL;
	
	int* cfd = NULL;
	
	logging_t level = LOG_QUIET;
	
	system_error_t error = {0};
	
	memset(&sigpipe_action, 0, sizeof(sigpipe_action));
	
	sigpipe_action.sa_handler = SIG_IGN;
	
	curl_multi = curl_multi_init();
	
	if (curl_multi == NULL) {
		loggln(LOG_ERROR, "[error] could not initialize multi stack");
		
		return EXIT_FAILURE;
	}
	
	thread_create(&event_thread, event_loop, NULL);
	
	if (sigaction(SIGPIPE, &sigpipe_action, NULL) == -1) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not set signal handler: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	code = argparse_init(&argparse, argc, argv);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
		
		return EXIT_FAILURE;
	}
	
	while (1) {
		argument = argparse_getnext(&argparse);
		
		if (argument == NULL) {
			break;
		}
		
		if (strcmp(argument->key, "host") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(address) - 1)) {
				loggln(LOG_ERROR, "[error] address string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(address, argument->value);
		} else if (strcmp(argument->key, "port") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			value = atoi(argument->value);
			
			if (!(value >= 1 && value <= 65535)) {
				loggln(LOG_ERROR, "[error] bad port number: %i", value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			port = value;
		} else if (strcmp(argument->key, "target") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(target_impersonate) - 1)) {
				loggln(LOG_ERROR, "[error] target_impersonate string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(target_impersonate, argument->value);
		} else if (strcmp(argument->key, "loglevel") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			size = strlen(argument->value);
			
			if (size > (sizeof(loglevel) - 1)) {
				loggln(LOG_ERROR, "[error] loglevel string exceeds max buffer size: %s", argument->value);
				
				argparse_free(&argparse);
				
				return EXIT_FAILURE;
			}
			
			strcpy(loglevel, argument->value);
		} else if (strcmp(argument->key, "proxy") == 0 || strcmp(argument->key, "proxy-url") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(proxy_url) - 1)) {
				loggln(LOG_ERROR, "[error] proxy url exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(proxy_url, argument->value);
		} else if (strcmp(argument->key, "doh-url") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			size = strlen(argument->value);

			if (size > (sizeof(doh_url) - 1)) {
				loggln(LOG_ERROR, "[error] doh url exceeds max buffer size: %s", argument->value);

				argparse_free(&argparse);

				return EXIT_FAILURE;
			}

			strcpy(doh_url, argument->value);
		} else if (strcmp(argument->key, "v") == 0 || strcmp(argument->key, "version") == 0) {
			argparse_free(&argparse);
			
			printf("%s v%s (+%s)\n", KAD_NAME, KAD_VERSION, KAD_REPOSITORY);
			
			return EXIT_SUCCESS;
		} else if (strcmp(argument->key, "h") == 0 || strcmp(argument->key, "help") == 0) {
			argparse_free(&argparse);
			
			printf("%s\n", KAD_DESCRIPTION);
			
			return EXIT_SUCCESS;
		}
	}
	
	argparse_free(&argparse);
	
	if (*address == '\0') {
		strcpy(address, KAD_DEFAULT_LISTEN_ADDRESS);
	}
	
	if (port == 0) {
		port = KAD_DEFAULT_LISTEN_PORT;
	}
	
	if (*target_impersonate == '\0') {
		strcpy(target_impersonate, KAD_DEFAULT_IMPERSONATE_TARGET);
	}
	
	level = loglevel_unstringify(loglevel);
	
	if (level == LOG_STANDARD && strcmp(loglevel, "standard") != 0) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_LOGGING_INVALID_LEVEL), loglevel);
		
		return EXIT_FAILURE;
	}
	
	loglevel_set(level);
	
	#if !defined(KAD_DISABLE_SSL_VERIFY)
		err = load_ssl_certificates();
		
		if (err != KADERR_SUCCESS) {
			loggln(LOG_ERROR, "[error] could not load CA bundle: %s", strkaderr(err));
			
			return err;
		}
	#endif
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (fd == -1) {
		error = get_system_error();
		loggln(LOG_ERROR, "[error] could not create socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	memset(&lingerv, 0, sizeof(lingerv));
	
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &lingerv, sizeof(lingerv));
	
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
	
	memset(&addr_in, 0, sizeof(addr_in));
	
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(port);
	
	if (inet_pton(addr_in.sin_family, address, &addr_in.sin_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in;
		addrsize = sizeof(addr_in);
	}
	
	memset(&addr_in6, 0, sizeof(addr_in6));
	
	addr_in6.sin6_family = AF_INET6;
	addr_in6.sin6_port = htons(port);
	
	if (inet_pton(addr_in6.sin6_family, address, &addr_in6.sin6_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in6;
		addrsize = sizeof(addr_in6);
	}
	
	if (sockaddress == NULL) {
		loggln(LOG_ERROR, "[error] invalid address: %s", address);
		
		return EXIT_FAILURE;
	}
	
	if (bind(fd, sockaddress, addrsize) == -1) {
		error = get_system_error();
		close(fd);
		loggln(LOG_ERROR, "[error] could not bind socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	if (listen(fd, KAD_DEFAULT_LISTEN_BACKLOG) == -1) {
		error = get_system_error();
		close(fd);
		loggln(LOG_ERROR, "[error] could not listen on socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	loggln(LOG_STANDARD, "Starting server at http://%s:%i (pid = %i)", address, port, getpid());
	
	while (1) {
		memset(&client_address, 0, sizeof(client_address));
		
		client_address_size = sizeof(client_address);
		
		cfd = malloc(sizeof(*cfd));
		
		if (cfd == NULL) {
			error = get_system_error();
			close(fd);
			loggln(LOG_ERROR, "[error] could not allocate memory for client socket: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		*cfd = accept(fd, (struct sockaddr*) &client_address, &client_address_size);
		
		if (*cfd == -1) {
			error = get_system_error();
			close(fd);
			loggln(LOG_ERROR, "[error] could not accept incoming socket connection: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		status = getnameinfo((struct sockaddr*) &client_address, client_address_size, host, sizeof(host), servname, sizeof(servname), NI_NUMERICHOST | NI_NUMERICSERV);
		
		if (status != 0) {
			close(fd);
			loggln(LOG_ERROR, "[error] could not get address info: %s", gai_strerror(status));
			
			return EXIT_FAILURE;
		}
		
		loggln(LOG_INFO, "[info] got connection from %s on port %s", host, servname);
		
		memset(&client_thread, 0, sizeof(client_thread));
		
		thread_create(&client_thread, handle_request, (void*) cfd);
		threads[position++] = client_thread;
		
		if (position < KAD_DEFAULT_LISTEN_BACKLOG) {
			continue;
		}
		
		loggln(LOG_WARN, "[warn] max number of concurrent connections reached");
		
		index = 0;
		
		while (index < position) {
			worker = &threads[index++];
			thread_wait(worker);
		}
		
		position = 0;
	}
	
	return EXIT_SUCCESS;
	
}

#if !defined(KAD_BUILD_SHARED)
int main(int argc, char* argv[]) {
	
	return kad_main(argc, argv);
	
}
#endif