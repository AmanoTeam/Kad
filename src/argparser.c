#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include <windows.h>
#endif

#include "argparser.h"

void argparser_init(struct ArgumentParser* const argparser, const int argc, argv_t** const argv) {
	
	argparser->index = 1;
	argparser->argc = (size_t) argc;
	argparser->argv = argv;
	
}

const struct Argument* argparser_next(struct ArgumentParser* const argparser) {
	
	if (argparser->argument.key != NULL) {
		free(argparser->argument.key);
		argparser->argument.key = NULL;
	}
	
	if (argparser->argument.value != NULL) {
		free(argparser->argument.value);
		argparser->argument.value = NULL;
	}
	
	if (argparser->index >= argparser->argc) {
		return NULL;
	}
	
	#if defined(_WIN32) && defined(_UNICODE)
		const wchar_t* const witem = argparser->argv[argparser->index++];
		
		const int items = WideCharToMultiByte(CP_UTF8, 0, witem, -1, NULL, 0, NULL, NULL);
		
		if (items == 0) {
			return NULL;
		}
		
		char item[(size_t) items];
		
		if (WideCharToMultiByte(CP_UTF8, 0, witem, -1, item, items, NULL, NULL) == 0) {
			return NULL;
		}
	#else
		const char* const item = argparser->argv[argparser->index++];
	#endif
	
	const char* const argument_start = item;
	const char* const argument_end = strchr(item, '\0');
	
	const char* key_start = argument_start;
	const char* key_end = strstr(argument_start, "=");
	
	if (key_end == NULL) {
		key_end = argument_end;
	}
	
	while (*key_start == '-') {
		key_start++;
	}
	
	const size_t key_size = (size_t) (key_end - key_start);
	
	argparser->argument.key = malloc(key_size + 1);
	
	if (argparser->argument.key == NULL) {
		return NULL;
	}
	
	memcpy(argparser->argument.key, key_start, key_size);
	argparser->argument.key[key_size] = '\0';
	
	const char* value_start = key_end;
	
	if (key_end != argument_end) {
		value_start += 1;
	}
	
	const char* value_end = argument_end;
	
	const size_t value_size = (size_t) (value_end - value_start);
	
	if (value_size > 0) {
		argparser->argument.value = malloc(value_size + 1);
		
		if (argparser->argument.value == NULL) {
			return NULL;
		}
		
		memcpy(argparser->argument.value, value_start, value_size);
		argparser->argument.value[value_size] = '\0';
	}
	
	return &argparser->argument;
	
}
