#if !defined(LOGGING_H)
#define LOGGING_H

#include <stdarg.h>

enum Logging {
	LOG_QUIET = (0x000000000),
	LOG_STANDARD = (0x000000001),
	LOG_WARN = (0x000000002),
	LOG_ERROR = (0x000000004),
	LOG_INFO = (0x000000010),
	LOG_VERBOSE = (0x000000020)
};

typedef enum Logging logging_t;

void loggln(
	const logging_t type,
	const char* const format,
	...
);

void logg(
	const logging_t type,
	const char* const format,
	...
);

void loglevel_set(const logging_t type);
logging_t loglevel_get(void);

logging_t loglevel_unstringify(const char* const name);

#endif
