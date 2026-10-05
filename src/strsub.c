#include <stdlib.h>
#include <string.h>

static char* replace_string(const char* const pattern, const char* const replacement, const char* const string) {
	
	const char* match = NULL;
	char* value = NULL;
	
	size_t size = 0;
	size_t plen = strlen(pattern);
	
	if (*pattern == '\0') {
		return NULL;
	}
	
	if (strcmp(pattern, replacement) == 0) {
		return NULL;
	}
	
	match = strstr(string, pattern);
	
	if (match == NULL) {
		return strdup(string);
	}
	
	size = (strlen(string) + strlen(replacement) + 1) - plen;
	
	value = malloc(size);
	
	if (value == NULL) {
		return NULL;
	}
	
	size = (size_t) (match - string);
	
	memcpy(value, string, size);
	value[size] = '\0';
	
	strcat(value, replacement);
	
	size += plen;
	strcat(value, string + size);
	
	return value;
	
}

char* strsub(const char* pattern, const char* replacement, const char* string) {

	char* cur = NULL;
	char* next = NULL;
	
	cur = strdup(string);
	
	if (cur == NULL) {
		return NULL;
	}

	while (strstr(cur, pattern) != NULL) {
		next = replace_string(pattern, replacement, cur);
		
		free(cur);

		if (next == NULL) {
			return NULL;
		}

		cur = next;
	}

	return cur;
	
}
