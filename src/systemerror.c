#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <errno.h>
#endif

#include "systemerror.h"

system_error_t get_system_error(void) {
	/*
	Returns a description of the last error reported by the operating system.
	*/
	
	system_error_t error = {0};
	
	#if defined(_WIN32)
		const DWORD code = GetLastError();
		
		#if defined(_UNICODE)
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
		const char* const message = strerror(code);
		
		error.code = code;
		
		if (message != NULL) {
			strcpy(error.message, message);
		} else {
			strcpy(error.message, "Unknown error");
		}
	#endif
	
	return error;
	
}
