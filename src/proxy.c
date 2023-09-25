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
#include "argparser.h"
#include "kad.h"
#include "threads.h"

#if !defined(KAD_DISABLE_CERTIFICATE_VALIDATION)
	#include "filesystem.h"
	#include "fstream.h"
	#include "stringu.h"
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

#ifndef KAD_DISABLE_CERTIFICATE_VALIDATION
	static const char CA_CERT_FILENAME[] = 
		PATH_SEPARATOR
		"etc"
		PATH_SEPARATOR
		"tls"
		PATH_SEPARATOR
		"cert.pem";
	
	static struct curl_blob curl_blob_global = {0};
#endif

static int fd = 0;

void sigint_handler() {
	
	close(fd);
	exit(EXIT_SUCCESS);
	
}

static char target_impersonate[16] = {0};

#ifndef KAD_DISABLE_CERTIFICATE_VALIDATION
	static int load_ssl_certificates(void) {
		
		char app_filename[PATH_MAX];
		
		if (get_app_filename(app_filename) == NULL) {
			return KADERR_FILESYSTEM_FAILURE;
		}
		
		char app_root_directory[PATH_MAX];
		get_parent_directory(app_filename, app_root_directory, 2);
		
		char ca_bundle[strlen(app_root_directory) + strlen(CA_CERT_FILENAME) + 1];
		strcpy(ca_bundle, app_root_directory);
		strcat(ca_bundle, CA_CERT_FILENAME);
		
		struct FStream* const stream = fstream_open(ca_bundle, FSTREAM_READ);
		
		if (stream == NULL) {
			return KADERR_FSTREAM_OPEN_FAILURE;
		}
		
		if (fstream_seek(stream, 0, FSTREAM_SEEK_END) == -1) {
			return KADERR_FSTREAM_SEEK_FAILURE;
		}
		
		const long int file_size = fstream_tell(stream);
		
		if (file_size == -1) {
			return KADERR_FSTREAM_TELL_FAILURE;
		}
		
		if (fstream_seek(stream, 0, FSTREAM_SEEK_BEGIN) == -1) {
			return KADERR_FSTREAM_SEEK_FAILURE;
		}
		
		curl_blob_global.data = malloc((size_t) file_size);
		
		if (curl_blob_global.data == NULL) {
			return KADERR_MEMORY_ALLOCATE_FAILURE;
		}
		
		const ssize_t size = fstream_read(stream, curl_blob_global.data, (size_t) file_size);
		
		if (size == -1) {
			return KADERR_FSTREAM_READ_FAILURE;
		}
		
		fstream_close(stream);
		
		curl_blob_global.len = (size_t) file_size;
		
		return KADERR_SUCCESS;
		
	}
#endif

static int request_handler(void* pointer) {
	
	int fd __close__ = *(int*) pointer;
	
	struct SSLContext context __ssl_close__ = {0};
	
	struct HTTPRequest request __http_request_free__ = {0};
	http_request_init(&request);
	
	struct HTTPResponse response __http_response_free__ = {0};
	http_response_init(&response);
	
	char buffer[MAX_HTTP_HEADERS_SIZE];
	const ssize_t recv_size = recv(fd, buffer, MAX_HTTP_HEADERS_SIZE, 0);
	
	int rc = http_request_parse(&request, buffer, (size_t) recv_size);
	
	if (rc == -1) {
		return KADERR_SOCKET_RECV_FAILURE;
	}
	
	const int is_secure = (request.method == CONNECT);
	
	struct transferdata data = {
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
		
		if (size == -1) {
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
	
	printf("[info] client request to %s\n", request.uri);
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	CURL* curl __curl_easy_cleanup__ = curl_easy_init();
	
	if (curl == NULL) {
		return KADERR_CURL_INIT_FAILURE;
	}
	
	if (curl_easy_impersonate(curl, target_impersonate, 1) != CURLE_OK) {
		return KADERR_CURL_SETOPT_FAILURE;
	}
	
	#if defined(KAD_DISABLE_CERTIFICATE_VALIDATION)
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
	
	struct curl_slist* list __curl_slist_free_all__ = NULL;
	
	for (size_t index = 0; index < request.headers.offset; index++) {
		const struct HTTPHeader* const header = &request.headers.items[index];
		
		int matches = 0;
		
		for (size_t index = 0; index < sizeof(IMPERSONATE_HEADERS) / sizeof(*IMPERSONATE_HEADERS); index++) {
			const char* const name = IMPERSONATE_HEADERS[index];
			
			matches = strcasecmp(header->key, name) == 0;
			
			if (matches) {
				break;
			}
		}
		
		if (matches) {
			fprintf(stderr, "[warning] ignoring client header '%s' to avoid conflicts with curl-impersonate\n", header->key);
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
	
	if (request.body.size > 0 || recv_size >= MAX_HTTP_HEADERS_SIZE) {
		if (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		if (curl_easy_setopt(curl, CURLOPT_READDATA, (void*) &data) != CURLE_OK) {
			return KADERR_CURL_SETOPT_FAILURE;
		}
		
		const struct HTTPHeader* const item = http_headers_get(&request.headers, "Transfer-Encoding");
		
		const int use_chunked = (item != NULL && strcmp(item->value, "chunked") == 0);
		
		if (!use_chunked) {
			const struct HTTPHeader* const item = http_headers_get(&request.headers, "Content-Length");
			
			if (item != NULL) {
				const long int content_length = strtol(item->value, NULL, 10);
				
				if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, content_length) != CURLE_OK) {
					return KADERR_CURL_SETOPT_FAILURE;
				}
			}
		}
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
		fprintf(stderr, "[error] curl: %s\n", message);
		
		return KADERR_CURL_PERFORM_FAILURE;
	}
	
	return KADERR_SUCCESS;
	
}

static void* handle_request(void* pointer) {
	
	const int code = request_handler(pointer);
	
	if (code != KADERR_SUCCESS) {
		fprintf(stderr, "[error] %s\n", strkaderr(code));
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
	
	char address[512] = {0};
	int port = 0;
	
	struct ArgumentParser argparser = {0};
	argparser_init(&argparser, argc, argv);
	
	while (1) {
		const struct Argument* const argument = argparser_next(&argparser);
		
		if (argument == NULL) {
			break;
		}
		
		if (strcmp(argument->key, "address") == 0) {
			if (argument->value == NULL) {
				fprintf(stderr, "fatal error: missing required value for argument: --%s\n", argument->key);
				return EXIT_FAILURE;
			}
			
			const size_t size = strlen(argument->value);
			
			if (size > (sizeof(address) - 1)) {
				fprintf(stderr, "fatal error: address string exceeds max buffer size: %s\n", argument->value);
				return EXIT_FAILURE;
			}
			
			strcpy(address, argument->value);
		} else if (strcmp(argument->key, "port") == 0) {
			if (argument->value == NULL) {
				fprintf(stderr, "fatal error: missing required value for argument: --%s\n", argument->key);
				return EXIT_FAILURE;
			}
			
			const int value = atoi(argument->value);
			
			if (!(value >= 1 && value <= 65535)) {
				fprintf(stderr, "fatal error: bad port number: %i\n", value);
				return EXIT_FAILURE;
			}
			
			port = value;
		} else if (strcmp(argument->key, "target") == 0) {
			if (argument->value == NULL) {
				fprintf(stderr, "fatal error: missing required value for argument: --%s\n", argument->key);
				return EXIT_FAILURE;
			}
			
			const size_t size = strlen(argument->value);
			
			if (size > (sizeof(target_impersonate) - 1)) {
				fprintf(stderr, "fatal error: address string exceeds max buffer size: %s\n", argument->value);
				return EXIT_FAILURE;
			}
			
			strcpy(target_impersonate, argument->value);
		} else if (strcmp(argument->key, "version") == 0) {
			printf("%s v%s (+%s)\n", KAD_NAME, KAD_VERSION, KAD_REPOSITORY);
			return EXIT_SUCCESS;
		} else if (strcmp(argument->key, "help") == 0) {
			printf("%s\n", KAD_DESCRIPTION);
			return EXIT_SUCCESS;
		}
	}
	
	if (*address == '\0') {
		strcpy(address, KAD_DEFAULT_LISTEN_ADDRESS);
	}
	
	if (port == 0) {
		port = KAD_DEFAULT_LISTEN_PORT;
	}
	
	if (*target_impersonate == '\0') {
		strcpy(target_impersonate, KAD_DEFAULT_IMPERSONATE_TARGET);
	}
	
	const int code = load_ssl_certificates();
	
	if (code != KADERR_SUCCESS) {
		fprintf(stderr, "fatal error: could not load CA bundle: %s\n", strkaderr(code));
		return code;
	}
	
	fd = socket(AF_INET, SOCK_STREAM, 0);
	
	if (fd == -1) {
		const struct SystemError error = get_system_error();
		fprintf(stderr, "fatal error: could not create socket: %s\n", error.message);
		
		return EXIT_FAILURE;
	}
	
	const struct linger lingerv = {
		.l_onoff = 0,
		.l_linger = 0
	};
	
	setsockopt(fd, SOL_SOCKET, SO_LINGER, &lingerv, sizeof(lingerv));
	
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
		fprintf(stderr, "fatal error: invalid address: %s\n", KAD_DEFAULT_LISTEN_ADDRESS);
		return EXIT_FAILURE;
	}
	
	if (bind(fd, sockaddress, addrsize) == -1) {
		const struct SystemError error = get_system_error();
		close(fd);
		fprintf(stderr, "fatal error: could not bind socket: %s\n", error.message);
		
		return EXIT_FAILURE;
	}
	
	if (listen(fd, KAD_DEFAULT_LISTEN_BACKLOG) == -1) {
		const struct SystemError error = get_system_error();
		close(fd);
		fprintf(stderr, "fatal error: could not listen on socket: %s\n", error.message);
		
		return EXIT_FAILURE;
	}
	
	printf("Starting server at http://%s:%i (pid = %i)\n\n", address, port, getpid());
	
	thread_t threads[KAD_DEFAULT_LISTEN_BACKLOG];
	int position = 0;
	
	while (1) {
		struct sockaddr_storage address = {0};
		socklen_t size = sizeof(address);
		
		const int cfd = accept(fd, (struct sockaddr*) &address, &size);
		
		if (cfd == -1) {
			const struct SystemError error = get_system_error();
			close(fd);
			fprintf(stderr, "fatal error: could not accept incoming socket connection: %s\n", error.message);
			
			return EXIT_FAILURE;
		}
		
		char host[NI_MAXHOST];
		char port[NI_MAXSERV];
		
		const int code = getnameinfo((struct sockaddr*) &address, size, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
		
		if (code != 0) {
			close(fd);
			fprintf(stderr, "fatal error: could not get address info: %s\n", gai_strerror(code));
			
			return EXIT_FAILURE;
		}
		
		printf("[info] got connection from %s on port %s\n", host, port);
		
		thread_t thread = {0};
		thread_create(&thread, handle_request, (void*) &cfd);
		threads[position++] = thread;
		
		if (position < KAD_DEFAULT_LISTEN_BACKLOG) {
			continue;
		}
		
		fprintf(stderr, "[warning] max number of concurrent connections reached\n");
		
		int index = 0;
		
		while (index < position) {
			thread_t* const thread = &threads[index++];
			thread_wait(thread);
		}
		
		position = 0;
	}
	
	return 0;
	
}