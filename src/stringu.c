#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32)
	#include <sys/types.h>
#endif

#include "stringu.h"
#include "constants.h"

char* get_parent_directory(const char* const source, char* const destination, const size_t depth) {
	/*
	Returns the parent directory of a path.
	*/
	
	const size_t len = strlen(source);
	size_t current_depth = 1;
	
	for (ssize_t index = (ssize_t) (len - 1); index >= 0; index--) {
		const char* const ch = &source[index];
		
		if (*ch == *PATH_SEPARATOR && current_depth++ == depth) {
			const size_t size = (size_t) (ch - source);
			
			if (size > 0) {
				memcpy(destination, source, size);
				destination[size] = '\0';
			} else {
				strcpy(destination, PATH_SEPARATOR);
			}
			
			break;
		}
		
		if (index == 0 && len > 2 && isalpha((unsigned char) *source) && source[1] == *COLON && source[2] == *PATH_SEPARATOR) {
			memcpy(destination, source, 3);
			destination[3] = '\0';
		}
	}
	
	return destination;
	
}
