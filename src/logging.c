#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#if defined(__ANDROID__)
	#include <android/log.h>

	#define LOGGING_TAG "kad"
#endif

#include "logging.h"

static logging_t loglevel = LOG_VERBOSE;

#if defined(__ANDROID__)

static android_LogPriority logging_priority(const logging_t type) {
	
	switch (type) {
		case LOG_STANDARD:
		case LOG_INFO:
			return ANDROID_LOG_INFO;
		case LOG_WARN:
			return ANDROID_LOG_WARN;
		case LOG_ERROR:
			return ANDROID_LOG_ERROR;
		case LOG_VERBOSE:
			return ANDROID_LOG_VERBOSE;
		default:
			return ANDROID_LOG_SILENT;
	}
	
}

#endif

static void logging(
	const logging_t type,
	const int ln,
	const char* const format,
	va_list args
) {
	
	if (type <= loglevel || ((type == LOG_WARN || type == LOG_ERROR) && loglevel != LOG_QUIET)) {
#if defined(__ANDROID__)
		(void) ln;
		
		__android_log_vprint(logging_priority(type), LOGGING_TAG, format, args);
#else
		vfprintf(stderr, format, args);
		
		if (ln) {
			fprintf(stderr, "\n");
		}
		
		fflush(stderr);
#endif
	}
	
}

void loggln(
	const logging_t type,
	const char* const format,
	...
) {
	
	va_list list;
	va_start(list, format);
	
	logging(type, 1, format, list);
	
	va_end(list);
	
}

void logg(
	const logging_t type,
	const char* const format,
	...
) {
	
	va_list list;
	va_start(list, format);
	
	logging(type, 0, format, list);
	
	va_end(list);
	
}


void loglevel_set(const logging_t type) {
	
	loglevel = type;
	
}

logging_t loglevel_get(void) {
	
	return loglevel;
	
}

logging_t loglevel_unstringify(const char* const name) {
	
	if (strcmp(name, "quiet") == 0) {
		return LOG_QUIET;
	}
	
	if (strcmp(name, "standard") == 0) {
		return LOG_STANDARD;
	}
	
	if (strcmp(name, "warning") == 0) {
		return LOG_WARN;
	}
	
	if (strcmp(name, "error") == 0) {
		return LOG_ERROR;
	}
	
	if (strcmp(name, "info") == 0) {
		return LOG_INFO;
	}
	
	if (strcmp(name, "verbose") == 0) {
		return LOG_VERBOSE;
	}
	
	return LOG_STANDARD;
	
}

	