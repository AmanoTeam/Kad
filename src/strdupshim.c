#include <stdlib.h>
#include <string.h>

#undef strdup
#undef strndup

char* strdup(const char* string) {

	size_t size = strlen(string) + 1;

	char* value = malloc(size);

	if (value == NULL) {
		return NULL;
	}

	memcpy(value, string, size);

	return value;

}

char* strndup(const char* string, size_t size) {

	size_t length = strnlen(string, size);

	char* value = malloc(length + 1);

	if (value == NULL) {
		return NULL;
	}

	memcpy(value, string, length);

	value[length] = '\0';

	return value;

}
