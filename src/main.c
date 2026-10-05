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
static const int KAD_DEFAULT_LISTEN_PORT = 4000;

static const int KAD_DEFAULT_LISTEN_BACKLOG = 100;

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

static void sigint_handler(void) {
	
	close(fd);
	exit(EXIT_SUCCESS);
	
}

static char target_impersonate[64] = {0};

#if !defined(KAD_DISABLE_SSL_VERIFY)
static int load_ssl_certificates(void) {
	/*
	Loads the CA certificate bundle into memory for cURL to use.
	
	Returns (0) on success, (-1) on error.
	*/
	
	char* app_directory = get_app_directory();
	
	if (app_directory == NULL) {
		loggln(LOG_ERROR, "[error] could not get application directory: %s", strkaderr(KADERR_FS_GET_APP_DIRECTORY_FAILURE));
		
		return KADERR_FS_GET_APP_DIRECTORY_FAILURE;
	}
	
	char ca_bundle[strlen(app_directory) + strlen(CA_CERT_FILENAME) + 1];
	strcpy(ca_bundle, app_directory);
	strcat(ca_bundle, CA_CERT_FILENAME);
	
	free(app_directory);
	
	fstream_t* stream = fstream_open(ca_bundle, FSTREAM_READ);
	
	if (stream == NULL) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not open file at '%s': %s", ca_bundle, error.message);
		
		return KADERR_FSTREAM_OPEN_FAILURE;
	}
	
	const int64_t file_size = fstream_size(stream);
	
	if (file_size == FSTREAM_ERROR) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not get file size of '%s': %s", ca_bundle, error.message);
		
		fstream_close(stream);
		
		return KADERR_FSTREAM_TELL_FAILURE;
	}
	
	if (file_size == 0) {
		loggln(LOG_ERROR, "[error] file at '%s' is empty", ca_bundle);
		
		fstream_close(stream);
		
		return KADERR_FSTREAM_READ_EMPTY_FILE;
	}
	
	curl_blob_global.data = malloc((size_t) file_size);
	
	if (curl_blob_global.data == NULL) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not allocate memory: %s", error.message);
		
		fstream_close(stream);
		
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	const ssize_t size = fstream_read(stream, curl_blob_global.data, (size_t) file_size);
	
	if (size == FSTREAM_ERROR) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not read contents of file at '%s': %s", ca_bundle, error.message);
		
		fstream_close(stream);
		
		return KADERR_FSTREAM_READ_FAILURE;
	}
	
	if (fstream_close(stream) == FSTREAM_ERROR) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not close file at '%s': %s", ca_bundle, error.message);
		
		return KADERR_FSTREAM_CLOSE_FAILURE;
	}
	
	curl_blob_global.len = (size_t) size;
	
	return KADERR_SUCCESS;
	
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
	
	(void) pointer;
	
	int left = 0;
	CURLMsg* msg = NULL;
	
	CURLMcode code = CURLM_OK;
	CURLcode easy_code = 0;
	
	curl_pending_t* item = NULL;
	transferdata_t* data = NULL;
	
	int running = 0;
	
	int err = 0;

	while (1) {
		pthread_mutex_lock(&curl_pending_mutex);
		item = curl_pending;
		curl_pending = NULL;
		pthread_mutex_unlock(&curl_pending_mutex);
		
		while (item != NULL) {
			curl_pending_t* const next = item->next;
			
			CURL* const handle = item->handle;
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
	
	end:
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
	free(pointer);

	transferdata_t* data __transferdata_close__ = NULL;
	
	data = malloc(sizeof(*data));
	
	if (data == NULL) {
		close(fd);
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	memset(data, 0, sizeof(*data));
	data->fd = fd;
	
	const http_header_t* header = NULL;
	
	char buffer[MAX_HTTP_HEADERS_SIZE];
	const ssize_t recv_size = recv(fd, buffer, MAX_HTTP_HEADERS_SIZE, 0);
	
	if (recv_size <= 0) {
		return KADERR_SOCKET_RECV_FAILURE;
	}
	
	http_request_init(&data->request);
	
	err = http_request_parse(&data->request, buffer, (size_t) recv_size);
	
	if (err != KADERR_SUCCESS) {
		return err;
	}
	
	const int is_secure = (data->request.method == CONNECT);
	
	data->is_secure = is_secure;

	if (is_secure) {
		ssize_t size = send(fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
		
		if (size == -1) {
			return KADERR_SOCKET_SEND_FAILURE;
		}
		
		if (ssl_init(&data->context, &fd) == -1) {
			return KADERR_SSL_INIT_FAILURE;
		}
		
		size = ssl_recv(&data->context, buffer, sizeof(buffer));
		
		if (size <= 0) {
			return KADERR_SSL_RECV_FAILURE;
		}
		
		char hostname[strlen(data->request.uri) + 1];
		strcpy(hostname, data->request.uri);
		
		http_request_free(&data->request);
		
		err = http_request_parse(&data->request, buffer, (size_t) size);
		
		if (err != KADERR_SUCCESS) {
			return err;
		}
		
		char* uri = malloc(strlen(HTTPS_SCHEME) + strlen(SCHEME_SEPARATOR) + strlen(hostname) + strlen(data->request.uri) + 1);
		
		if (uri == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(uri, HTTPS_SCHEME);
		strcat(uri, SCHEME_SEPARATOR);
		strcat(uri, hostname);
		strcat(uri, data->request.uri);
		
		free(data->request.uri);
		data->request.uri = uri;
	}
	
	loggln(LOG_INFO, "[info] client request to %s", data->request.uri);
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	CURL* curl __curl_easy_cleanup__ = curl_easy_init();
	
	if (curl == NULL) {
		return KADERR_CURL_INIT_FAILURE;
	}
	
	if (curl_easy_impersonate(curl, target_impersonate, 1) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback_empty) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_ALTSVC, "") != CURLE_OK) {
		err = KADERR_CURL_SETOPT_FAILURE;
		goto end;
	}
	
	#if defined(KAD_DISABLE_SSL_VERIFY)
		if (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
	#else
		if (curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &curl_blob_global) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
	#endif
	
	static char curl_error_message[CURL_ERROR_SIZE] = {0};
	
	if (curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error_message) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	// Advertise all compression algorithms supported by curl and let it transparently decode the response body
	if (curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "") != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	for (size_t index = 0; index < data->request.headers.offset; index++) {
		header = &data->request.headers.items[index];
		
		int matches = 0;
		
		for (size_t index = 0; index < sizeof(IMPERSONATE_HEADERS) / sizeof(*IMPERSONATE_HEADERS); index++) {
			const char* const name = IMPERSONATE_HEADERS[index];
			
			matches = strcasecmp(header->key, name) == 0;
			
			if (matches) {
				break;
			}
		}
		
		if (matches) {
			loggln(LOG_WARN, "[warn] ignoring client header '%s' to avoid conflicts with curl-impersonate", header->key);
			continue;
		}
		
		char item[strlen(header->key) + strlen(HEADER_SEPARATOR) + strlen(header->value) + 1];
		strcpy(item, header->key);
		strcat(item, HEADER_SEPARATOR);
		strcat(item, header->value);
		
		struct curl_slist* const tmp = curl_slist_append(data->headers, item);
		
		if (tmp == NULL) {
			return KADERR_CURL_SLIST_FAILURE;
		}
		
		data->headers = tmp;
	}
	
	if (data->request.method == POST || data->request.method == PUT) {
		header = http_headers_get(&data->request.headers, "Expect");
		
		if (header == NULL) {
			struct curl_slist* const tmp = curl_slist_append(data->headers, "Expect:");
			
			if (tmp == NULL) {
				return KADERR_CURL_SLIST_FAILURE;
			}
			
			data->headers = tmp;
		}
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, data->headers) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_URL, data->request.uri) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (data->request.method != HEAD) {
		if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) data) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
	}
	
	header = http_headers_get(&data->request.headers, "Transfer-Encoding");
	
	const int use_chunked = (header != NULL && strcmp(header->value, "chunked") == 0);
	
	size_t content_length = 0;
	
	if (!use_chunked) {
		header = http_headers_get(&data->request.headers, "Content-Length");
		
		if (header != NULL) {
			content_length = strtoull(header->value, NULL, 10);
		}
	}
	
	if (data->request.body.size > 0 || content_length > 0 || use_chunked || recv_size >= MAX_HTTP_HEADERS_SIZE) {
		if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_READDATA, (void*) data) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (content_length > 0) {
			if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) content_length) != CURLE_OK) {
				return KADERR_CURL_SETOPT_FAILURE;
			}
		}
		
		data->remaining = content_length;
	}
	
	const char* const http_method = http_method_stringify(data->request.method);
	
	CURLcode value = 0;
	
	switch (data->request.method) {
		case GET:
			value = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
			break;
		case POST:
			value = curl_easy_setopt(curl, CURLOPT_POST, 1L);
			break;
		case HEAD:
			value = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			break;
		default:
			value = curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, http_method);
			break;
	}
	
	if (value != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*) data) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_PRIVATE, (void*) data) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	curl_pending_t* const item = malloc(sizeof(*item));
	
	if (item == NULL) {
		return KADERR_MEMORY_ALLOCATE_FAILURE;
	}
	
	item->handle = curl;
	
	pthread_mutex_lock(&curl_pending_mutex);
	item->next = curl_pending;
	curl_pending = item;
	pthread_mutex_unlock(&curl_pending_mutex);
	
	curl_multi_wakeup(curl_multi);
	
	curl = NULL;
	data = NULL;
	
	return KADERR_SUCCESS;
	
}

static void* handle_request(void* pointer) {
	
	const int code = request_handler(pointer);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
	}
	
	return NULL;
	
}

int main(int argc, char* argv[]) {
	/*
	const struct sigaction action = {
		.sa_handler = &sigint_handler
	};
	
	if (sigaction(SIGINT, &action, NULL) < 0) {
		abort_with_perror("sigaction()");
	}
	*/
	
	const struct sigaction sigpipe_action = {
		.sa_handler = SIG_IGN
	};
	
	curl_multi = curl_multi_init();
	
	if (curl_multi == NULL) {
		loggln(LOG_ERROR, "[error] could not initialize multi stack");
		
		return EXIT_FAILURE;
	}
	
	thread_t event_thread = {0};
	thread_create(&event_thread, event_loop, NULL);
	
	if (sigaction(SIGPIPE, &sigpipe_action, NULL) == -1) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not set signal handler: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	char address[512] = {0};
	int port = 0;
	
	char loglevel[16] = "verbose";
	
	argparse_t argparse = {0};
	
	const int code = argparse_init(&argparse, argc, argv);
	
	if (code != KADERR_SUCCESS) {
		loggln(LOG_ERROR, "[error] %s", strkaderr(code));
		
		return EXIT_FAILURE;
	}
	
	while (1) {
		const arg_t* const argument = argparse_getnext(&argparse);
		
		if (argument == NULL) {
			break;
		}
		
		if (strcmp(argument->key, "host") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				return EXIT_FAILURE;
			}
			
			const size_t size = strlen(argument->value);
			
			if (size > (sizeof(address) - 1)) {
				loggln(LOG_ERROR, "[error] address string exceeds max buffer size: %s", argument->value);
				
				return EXIT_FAILURE;
			}
			
			strcpy(address, argument->value);
		} else if (strcmp(argument->key, "port") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				return EXIT_FAILURE;
			}
			
			const int value = atoi(argument->value);
			
			if (!(value >= 1 && value <= 65535)) {
				loggln(LOG_ERROR, "[error] bad port number: %i", value);
				
				return EXIT_FAILURE;
			}
			
			port = value;
		} else if (strcmp(argument->key, "target") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				return EXIT_FAILURE;
			}
			
			const size_t size = strlen(argument->value);
			
			if (size > (sizeof(target_impersonate) - 1)) {
				loggln(LOG_ERROR, "[error] target_impersonate string exceeds max buffer size: %s", argument->value);
				
				return EXIT_FAILURE;
			}
			
			strcpy(target_impersonate, argument->value);
		} else if (strcmp(argument->key, "loglevel") == 0) {
			if (argument->value == NULL) {
				loggln(LOG_ERROR, "[error] %s: --%s", strkaderr(KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING), argument->key);
				
				return EXIT_FAILURE;
			}
			
			const size_t size = strlen(argument->value);
			
			if (size > (sizeof(loglevel) - 1)) {
				loggln(LOG_ERROR, "[error] loglevel string exceeds max buffer size: %s", argument->value);
				
				return EXIT_FAILURE;
			}
			
			strcpy(loglevel, argument->value);
		} else if (strcmp(argument->key, "v") == 0 || strcmp(argument->key, "version") == 0) {
			printf("%s v%s (+%s)\n", KAD_NAME, KAD_VERSION, KAD_REPOSITORY);
			
			return EXIT_SUCCESS;
		} else if (strcmp(argument->key, "h") == 0 || strcmp(argument->key, "help") == 0) {
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
	
	const logging_t level = loglevel_unstringify(loglevel);
	
	if (level == LOG_STANDARD && strcmp(loglevel, "standard") != 0) {
		loggln(LOG_ERROR, "[error] %s: %s", strkaderr(KADERR_LOGGING_INVALID_LEVEL), loglevel);
		
		return EXIT_FAILURE;
	}
	
	loglevel_set(level);
	
	#if !defined(KAD_DISABLE_SSL_VERIFY)
		const int err = load_ssl_certificates();
		
		if (err != KADERR_SUCCESS) {
			loggln(LOG_ERROR, "[error] could not load CA bundle: %s", strkaderr(err));
			
			return err;
		}
	#endif
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (fd == -1) {
		const system_error_t error = get_system_error();
		loggln(LOG_ERROR, "[error] could not create socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	const struct linger lingerv = {
		.l_onoff = 0,
		.l_linger = 0
	};
	
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &lingerv, sizeof(lingerv));
	
	const int reuseaddr = 1;
	
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
	
	struct sockaddr* sockaddress = NULL;
	size_t addrsize = 0;
	
	struct sockaddr_in addr_in = {
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};
	
	if (inet_pton(addr_in.sin_family, address, &addr_in.sin_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in;
		addrsize = sizeof(addr_in);
	}
	
	struct sockaddr_in6 addr_in6 = {
		.sin6_family = AF_INET6,
		.sin6_port = htons(port)
	};
	
	if (inet_pton(addr_in6.sin6_family, address, &addr_in6.sin6_addr) == 1) {
		sockaddress = (struct sockaddr*) &addr_in6;
		addrsize = sizeof(addr_in6);
	}
	
	if (sockaddress == NULL) {
		loggln(LOG_ERROR, "[error] invalid address: %s", address);
		
		return EXIT_FAILURE;
	}
	
	if (bind(fd, sockaddress, addrsize) == -1) {
		const system_error_t error = get_system_error();
		close(fd);
		loggln(LOG_ERROR, "[error] could not bind socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	if (listen(fd, KAD_DEFAULT_LISTEN_BACKLOG) == -1) {
		const system_error_t error = get_system_error();
		close(fd);
		loggln(LOG_ERROR, "[error] could not listen on socket: %s", error.message);
		
		return EXIT_FAILURE;
	}
	
	loggln(LOG_STANDARD, "Starting server at http://%s:%i (pid = %i)", address, port, getpid());
	
	thread_t threads[KAD_DEFAULT_LISTEN_BACKLOG];
	int position = 0;
	
	while (1) {
		struct sockaddr_storage address = {0};
		socklen_t size = sizeof(address);
		
		int* cfd = malloc(sizeof(*cfd));
		
		if (cfd == NULL) {
			const system_error_t error = get_system_error();
			close(fd);
			loggln(LOG_ERROR, "[error] could not allocate memory for client socket: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		*cfd = accept(fd, (struct sockaddr*) &address, &size);
		
		if (*cfd == -1) {
			const system_error_t error = get_system_error();
			close(fd);
			loggln(LOG_ERROR, "[error] could not accept incoming socket connection: %s", error.message);
			
			return EXIT_FAILURE;
		}
		
		char host[NI_MAXHOST];
		char port[NI_MAXSERV];
		
		const int code = getnameinfo((struct sockaddr*) &address, size, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
		
		if (code != 0) {
			close(fd);
			loggln(LOG_ERROR, "[error] could not get address info: %s", gai_strerror(code));
			
			return EXIT_FAILURE;
		}
		
		loggln(LOG_INFO, "[info] got connection from %s on port %s", host, port);
		
		thread_t thread = {0};
		thread_create(&thread, handle_request, (void*) cfd);
		threads[position++] = thread;
		
		if (position < KAD_DEFAULT_LISTEN_BACKLOG) {
			continue;
		}
		
		loggln(LOG_WARN, "[warn] max number of concurrent connections reached");
		
		int index = 0;
		
		while (index < position) {
			thread_t* const thread = &threads[index++];
			thread_wait(thread);
		}
		
		position = 0;
	}
	
	return EXIT_SUCCESS;
	
}