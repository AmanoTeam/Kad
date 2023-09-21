#include <stdlib.h>

#if defined(_WIN32) && defined(_UNICODE)
	#define argv_t wchar_t
#else
	#define argv_t char
#endif

struct Argument {
	char* key;
	char* value;
};

struct ArgumentParser {
	size_t index;
	size_t argc;
	argv_t** argv;
	struct Argument argument;
};

void argparser_init(struct ArgumentParser* const argparser, const int argc, argv_t** const argv);
const struct Argument* argparser_next(struct ArgumentParser* const argparser);

#pragma once
