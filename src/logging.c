#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include "logging.h"

static logging_t loglevel = LOG_VERBOSE;

static void logging(
	const logging_t type,
	const int ln,
	const char* const format,
	va_list args
) {
	
	if (type <= loglevel || ((type == LOG_WARN || type == LOG_ERROR) && loglevel != LOG_QUIET)) {
		vfprintf(stderr, format, args);
		
		if (ln) {
			fprintf(stderr, "\n");
		}
		
		fflush(stderr);
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

	