#define KADERR_SUCCESS 0 /* Success */

#define KADERR_MEMORY_ALLOCATE_FAILURE -1 /* Could not allocate memory */
#define KADERR_NOT_IMPLEMENTED -2 /* This method is not implemented */

#define KADERR_HTTP_MALFORMED_REQUEST -3 /* Malformed HTTP request */
#define KADERR_HTTP_HEADERS_TOO_BIG -4 /* HTTP headers exceeded max allowed size */
#define KADERR_HTTP_MALFORMED_HEADER -5 /* Malformed HTTP header */
#define KADERR_HTTP_HEADER_CONTAINS_INVALID_CHARACTER -6 /* HTTP header contains invalid character */
#define KADERR_HTTP_MISSING_HEADER_NAME -7 /* HTTP header name is empty */
#define KADERR_HTTP_MISSING_HEADER_VALUE -8 /* HTTP header value is empty */
#define KADERR_HTTP_UNKNOWN_METHOD -9 /* Unknown HTTP method */
#define KADERR_HTTP_UNSUPPORTED_VERSION -10 /* Unsupported HTTP version */

#define KADERR_SOCKET_SEND_FAILURE -11 /* Cannot send data on socket */
#define KADERR_SOCKET_RECV_FAILURE -12 /* Cannot receive data on socket */

#define KADERR_SSL_INIT_FAILURE -13 /* Failed to initialize the SSL engine */
#define KADERR_SSL_SEND_FAILURE -14 /* Cannot send data on SSL socket */
#define KADERR_SSL_RECV_FAILURE -15 /* Cannot receive data on SSL socket */

#define KADERR_CURL_INIT_FAILURE -16 /* Failed to initialize the cURL HTTP client */
#define KADERR_CURL_SETOPT_FAILURE -17 /* Failed to set option on the cURL HTTP client */
#define KADERR_CURL_SLIST_FAILURE -18 /* Could not append data to cURL array */
#define KADERR_CURL_PERFORM_FAILURE  -19 /* General cURL failure */

#define KADERR_FSTREAM_OPEN_FAILURE -20 /* Could not open file for read/write*/
#define KADERR_FSTREAM_SEEK_FAILURE -21 /* Could not seek on file */
#define KADERR_FSTREAM_TELL_FAILURE -22 /* Could not get current file position */
#define KADERR_FSTREAM_READ_FAILURE -23 /* Could not read file */
#define KADERR_FSTREAM_CLOSE_FAILURE -24 /* Could not close file */

#define KADERR_FILESYSTEM_FAILURE -25 /* Could not perform operation on the filesystem */

const char* strkaderr(const int code);

struct SystemError {
	int code;
	char message[256];
};

struct SystemError get_system_error(void);

#pragma once
