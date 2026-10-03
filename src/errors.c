/*
This file is auto-generated. Use the tool at ../tools/errors.h.py to regenerate.
*/

#include "errors.h"

const char* strkaderr(const int code) {
	
	switch (code) {
		case KADERR_SUCCESS:
			return "Success";
		case KADERR_ARGPARSE_ARGUMENT_EMPTY:
			return "Got an empty argument while parsing the command-line arguments";
		case KADERR_ARGPARSE_ARGUMENT_INVALID:
			return "This argument is invalid or was not recognized";
		case KADERR_ARGPARSE_ARGUMENT_VALUE_MISSING:
			return "This keyword argument requires a value to be supplied";
		case KADERR_ARGPARSE_VALUE_UNEXPECTED:
			return "Got an unexpected value while parsing the command-line arguments";
		case KADERR_CURL_INIT_FAILURE:
			return "Failed to initialize the cURL HTTP client";
		case KADERR_CURL_PERFORM_FAILURE:
			return "General cURL failure";
		case KADERR_CURL_SETOPT_FAILURE:
			return "Failed to set option on the cURL HTTP client";
		case KADERR_CURL_SLIST_FAILURE:
			return "Could not append data to cURL array";
		case KADERR_CURL_MULTI_PERFORM_FAILURE:
			return "Failed to perform transfers on the cURL multi stack";
		case KADERR_CURL_MULTI_POLL_FAILURE:
			return "Failed to poll for events on the cURL multi stack";
		case KADERR_CURL_MULTI_REMOVE_FAILURE:
			return "Failed to remove handle from the cURL multi stack";
		case KADERR_FSTREAM_CLOSE_FAILURE:
			return "Could not close file";
		case KADERR_FSTREAM_LOCK_FAILURE:
			return "Could not lock file";
		case KADERR_FSTREAM_OPEN_FAILURE:
			return "Could not open file";
		case KADERR_FSTREAM_READ_EMPTY_FILE:
			return "Tried to read contents from an empty file";
		case KADERR_FSTREAM_READ_FAILURE:
			return "Could not read data from file";
		case KADERR_FSTREAM_SEEK_FAILURE:
			return "Could not seek file";
		case KADERR_FSTREAM_TELL_FAILURE:
			return "Could not get current file position";
		case KADERR_FSTREAM_WRITE_FAILURE:
			return "Could not write data to file";
		case KADERR_FS_GET_APP_DIRECTORY_FAILURE:
			return "Could not get application directory";
		case KADERR_HTTP_HEADERS_TOO_BIG:
			return "HTTP headers exceeded max allowed size";
		case KADERR_HTTP_MALFORMED_REQUEST:
			return "Malformed HTTP request";
		case KADERR_HTTP_UNKNOWN_METHOD:
			return "Unknown HTTP method";
		case KADERR_HTTP_UNSUPPORTED_VERSION:
			return "Unsupported HTTP version";
		case KADERR_LOGGING_INVALID_LEVEL:
			return "Could not parse the log level";
		case KADERR_MEMORY_ALLOCATE_FAILURE:
			return "Could not allocate memory";
		case KADERR_SOCKET_RECV_FAILURE:
			return "Cannot receive data on socket";
		case KADERR_SOCKET_SEND_FAILURE:
			return "Cannot send data on socket";
		case KADERR_SSL_INIT_FAILURE:
			return "Failed to initialize the SSL engine";
		case KADERR_SSL_RECV_FAILURE:
			return "Cannot receive data on SSL socket";
		case KADERR_SSL_SEND_FAILURE:
			return "Cannot send data on SSL socket";
	}
	
	return "Unknown error";
	
}
