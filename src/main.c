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

static int request_handler(void* pointer) {
	
	int fd __close__ = *(int*) pointer;
	
	ssl_context_t context __ssl_close__ = {0};
	
	http_request_t request __http_request_free__ = {0};
	http_request_init(&request);
	
	http_response_t response __http_response_free__ = {0};
	http_response_init(&response);
	
	char buffer[MAX_HTTP_HEADERS_SIZE];
	const ssize_t recv_size = recv(fd, buffer, MAX_HTTP_HEADERS_SIZE, 0);
	
	if (recv_size <= 0) {
		return KADERR_SOCKET_RECV_FAILURE;
	}
	
	int rc = http_request_parse(&request, buffer, (size_t) recv_size);
	
	if (rc != KADERR_SUCCESS) {
		return rc;
	}
	
	const int is_secure = (request.method == CONNECT);
	
	transferdata_t data = {
		.context = &context,
		.request = &request,
		.response = &response,
		.fd = fd,
		.is_secure = is_secure
	};
	
	if (is_secure) {
		ssize_t size = send(fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
		
		if (size == -1) {
			return KADERR_SOCKET_SEND_FAILURE;
		}
		
		if (ssl_init(&context, &fd) == -1) {
			return KADERR_SSL_INIT_FAILURE;
		}
		
		size = ssl_recv(&context, buffer, sizeof(buffer));
		
		if (size <= 0) {
			return KADERR_SSL_RECV_FAILURE;
		}
		
		char hostname[strlen(request.uri) + 1];
		strcpy(hostname, request.uri);
		
		http_request_free(&request);
		
		const int code = http_request_parse(&request, buffer, (size_t) size);
		
		if (code != KADERR_SUCCESS) {
			return code;
		}
		
		char* uri = malloc(strlen(HTTPS_SCHEME) + strlen(SCHEME_SEPARATOR) + strlen(hostname) + strlen(request.uri) + 1);
		
		if (uri == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		strcpy(uri, HTTPS_SCHEME);
		strcat(uri, SCHEME_SEPARATOR);
		strcat(uri, hostname);
		strcat(uri, request.uri);
		
		free(request.uri);
		request.uri = uri;
	}
	
	loggln(LOG_INFO, "[info] client request to %s", request.uri);
	
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
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < request.headers.offset; index++) {
		const http_header_t* const header = &request.headers.items[index];
		
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
		
		struct curl_slist* const tmp = curl_slist_append(list, item);
		
		if (tmp == NULL) {
			return KADERR_CURL_SLIST_FAILURE;
		}
		
		list = tmp;
	}
	
	if (request.method == POST || request.method == PUT) {
		const http_header_t* const item = http_headers_get(&request.headers, "Expect");
		
		if (item == NULL) {
			struct curl_slist* const tmp = curl_slist_append(list, "Expect:");
			
			if (tmp == NULL) {
				return KADERR_CURL_SLIST_FAILURE;
			}
			
			list = tmp;
		}
	}
	
	/*
	if (curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 0L) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_HTTP_TRANSFER_DECODING, 0L) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	*/
	
	if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (curl_easy_setopt(curl, CURLOPT_URL, request.uri) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	if (request.method != HEAD) {
		if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*) &data) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
	}
	
	const http_header_t* const transfer_encoding = http_headers_get(&request.headers, "Transfer-Encoding");
	
	const int use_chunked = (transfer_encoding != NULL && strcmp(transfer_encoding->value, "chunked") == 0);
	
	size_t content_length = 0;
	
	if (!use_chunked) {
		const http_header_t* const content_length_header = http_headers_get(&request.headers, "Content-Length");
		
		if (content_length_header != NULL) {
			content_length = strtoull(content_length_header->value, NULL, 10);
		}
	}
	
	if (request.body.size > 0 || content_length > 0 || use_chunked || recv_size >= MAX_HTTP_HEADERS_SIZE) {
		if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_READDATA, (void*) &data) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (content_length > 0) {
			if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) content_length) != CURLE_OK) {
				return KADERR_CURL_SETOPT_FAILURE;
			}
		}
		
		data.remaining = content_length;
	}
	
	const char* const http_method = http_method_stringify(request.method);
	
	CURLcode value = 0;
	
	switch (request.method) {
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
	
	if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*) &data) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	const CURLcode status = curl_easy_perform(curl);
	
	if (status != CURLE_OK) {
		const char* const message = strlen(curl_error_message) > 0 ? curl_error_message : curl_easy_strerror(status);
		loggln(LOG_ERROR, "[error] curl: %s", message);
		
		return KADERR_CURL_PERFORM_FAILURE;
	}
	
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
		
		const int cfd = accept(fd, (struct sockaddr*) &address, &size);
		
		if (cfd == -1) {
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
		thread_create(&thread, handle_request, (void*) &cfd);
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