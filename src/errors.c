#if defined(_WIN32)
	#include <windows.h>
#endif

#if !defined(_WIN32)
	#include <errno.h>
	#include <string.h>
#endif

#include "errors.h"

const char* strkaderr(const int code) {
	
	switch (code) {
		case KADERR_SUCCESS:
			return "Success";
		case KADERR_MEMORY_ALLOCATE_FAILURE:
			return "Could not allocate memory";
		case KADERR_NOT_IMPLEMENTED:
			return "This method is not implemented";
		case KADERR_HTTP_MALFORMED_REQUEST:
			return "Malformed HTTP request";
		case KADERR_HTTP_HEADERS_TOO_BIG:
			return "HTTP headers exceeded max allowed size";
		case KADERR_HTTP_MALFORMED_HEADER:
			return "Malformed HTTP header";
		case KADERR_HTTP_HEADER_CONTAINS_INVALID_CHARACTER:
			return "HTTP header contains invalid character";
		case KADERR_HTTP_MISSING_HEADER_NAME:
			return "HTTP header is empty";
		case KADERR_HTTP_MISSING_HEADER_VALUE:
			return "HTTP header value is empty";
		case KADERR_HTTP_UNKNOWN_METHOD:
			return "Unknown HTTP method";
		case KADERR_HTTP_UNSUPPORTED_VERSION:
			return "Unsupported HTTP version";
		case KADERR_SOCKET_SEND_FAILURE:
			return "Cannot send data on socket";
		case KADERR_SOCKET_RECV_FAILURE:
			return "Cannot receive data on socket";
		case KADERR_SSL_INIT_FAILURE:
			return "Failed to initialize the SSL engine";
		case KADERR_SSL_SEND_FAILURE:
			return "Cannot send data on SSL socket";
		case KADERR_SSL_RECV_FAILURE:
			return "Cannot receive data on SSL socket";
		case KADERR_CURL_INIT_FAILURE:
			return "Failed to initialize the cURL HTTP client";
		case KADERR_CURL_SETOPT_FAILURE:
			return "Failed to set option on the cURL HTTP client";
		case KADERR_CURL_SLIST_FAILURE:
			return "Could not append data to cURL array";
		case KADERR_CURL_PERFORM_FAILURE:
			return "General cURL failure";
		case KADERR_FSTREAM_OPEN_FAILURE:
			return "Could not open file for read/write";
		case KADERR_FSTREAM_SEEK_FAILURE:
			return "Could not seek on file";
		case KADERR_FSTREAM_TELL_FAILURE:
			return "Could not get current file position";
		case KADERR_FSTREAM_READ_FAILURE:
			return "Could not read file";
		case KADERR_FSTREAM_CLOSE_FAILURE:
			return "Could not close file";
		case KADERR_FILESYSTEM_FAILURE:
			return "Could not perform operation on the filesystem";
		default:
			return "Unknown error";
	}
	
}

struct SystemError get_system_error(void) {
	
	struct SystemError error = {0};
	
	#ifdef _WIN32
		const DWORD code = GetLastError();
		
		#ifdef _UNICODE
			wchar_t wmessage[sizeof(error.message)];
			
			FormatMessageW(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				code,
				LANG_NEUTRAL,
				wmessage,
				sizeof(wmessage) / sizeof(*wmessage),
				NULL
			);
			
			WideCharToMultiByte(CP_UTF8, 0, wmessage, -1, error.message, sizeof(error.message), NULL, NULL);
		#else
			FormatMessageA(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				code,
				LANG_NEUTRAL,
				error.message,
				sizeof(error.message),
				NULL
			);
		#endif
		
		error.code = (int) code;
	#else
		const int code = errno;
		
		error.code = code;
		const char* const message = strerror(code);
		
		strcpy(error.message, message);
	#endif
	
	return error;
	
}
