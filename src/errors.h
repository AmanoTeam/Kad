#if !defined(ERRORS_H)
#define ERRORS_H

#define KADERR_SUCCESS 0 /* Success */

#define KADERR_ARGPARSE_ARGUMENT_EMPTY -1 /* Got an empty argument while parsing the command-line arguments */
#define KADERR_ARGPARSE_ARGUMENT_INVALID -2 /* This argument is invalid or was not recognized */
#define KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING -3 /* This keyword argument requires a value to be supplied */
#define KADERR_ARGPARSE_VALUE_UNEXPECTED -4 /* Got an unexpected value while parsing the command-line arguments */

#define KADERR_CURL_INIT_FAILURE -5 /* Failed to initialize the cURL HTTP client */
#define KADERR_CURL_PERFORM_FAILURE -6 /* General cURL failure */
#define KADERR_CURL_SETOPT_FAILURE -7 /* Failed to set option on the cURL HTTP client */
#define KADERR_CURL_SLIST_FAILURE -8 /* Could not append data to cURL array */

#define KADERR_CURL_MULTI_PERFORM_FAILURE -9 /* Failed to perform transfers on the cURL multi stack */
#define KADERR_CURL_MULTI_POLL_FAILURE -10 /* Failed to poll for events on the cURL multi stack */
#define KADERR_CURL_MULTI_REMOVE_FAILURE -11 /* Failed to remove handle from the cURL multi stack */

#define KADERR_FSTREAM_CLOSE_FAILURE -12 /* Could not close file */
#define KADERR_FSTREAM_LOCK_FAILURE -13 /* Could not lock file */
#define KADERR_FSTREAM_OPEN_FAILURE -14 /* Could not open file */
#define KADERR_FSTREAM_READ_EMPTY_FILE -15 /* Tried to read contents from an empty file */
#define KADERR_FSTREAM_READ_FAILURE -16 /* Could not read data from file */
#define KADERR_FSTREAM_SEEK_FAILURE -17 /* Could not seek file */
#define KADERR_FSTREAM_TELL_FAILURE -18 /* Could not get current file position */
#define KADERR_FSTREAM_WRITE_FAILURE -19 /* Could not write data to file */

#define KADERR_FS_GET_APP_DIRECTORY_FAILURE -20 /* Could not get application directory */

#define KADERR_HTTP_HEADERS_TOO_BIG -21 /* HTTP headers exceeded max allowed size */
#define KADERR_HTTP_MALFORMED_REQUEST -22 /* Malformed HTTP request */
#define KADERR_HTTP_UNKNOWN_METHOD -23 /* Unknown HTTP method */
#define KADERR_HTTP_UNSUPPORTED_VERSION -24 /* Unsupported HTTP version */

#define KADERR_LOGGING_INVALID_LEVEL -25 /* Could not parse the log level */

#define KADERR_MEMORY_ALLOCATE_FAILURE -26 /* Could not allocate memory */

#define KADERR_SOCKET_RECV_FAILURE -27 /* Cannot receive data on socket */
#define KADERR_SOCKET_SEND_FAILURE -28 /* Cannot send data on socket */

#define KADERR_SSL_INIT_FAILURE -29 /* Failed to initialize the SSL engine */
#define KADERR_SSL_RECV_FAILURE -30 /* Cannot receive data on SSL socket */
#define KADERR_SSL_SEND_FAILURE -31 /* Cannot send data on SSL socket */

#define APTERR_WCURL_GETINFO_FAILURE -5576 /* Could not get info about HTTP transfer */

const char* strkaderr(const int code);

#endif
