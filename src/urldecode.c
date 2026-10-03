#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "hex.h"
#include "urldecode.h"

static const char URI_SAFE_SYMBOLS[] = "!#$%&'()*+,-./:;=?@[]_~";

size_t urldecode(
	const char* const uri,
	char* const destination
) {
	
	size_t index = 0;
	size_t offset = 0;
	
	unsigned char a = 0;
	unsigned char b = 0;
	unsigned char c = 0;
	
	const size_t length = strlen(uri);
	
	for (index = 0; index < length; index++) {
		const unsigned char ch = uri[index];
		
		if (destination != NULL) {
			destination[offset] = ch;
		}
		
		offset++;
		
		if (ch == '%' && length > (index + 2)) {
			a = uri[index + 1];
			b = uri[index + 2];
			
			if (!(isxdigit(a) && isxdigit(b))) {
				continue;
			}
			
			c = ((from_hex(a) << 4) | from_hex(b));
			
			if (isalnum(c) || strchr(URI_SAFE_SYMBOLS, c) != NULL) {
				if (destination != NULL) {
					destination[offset - 1] = c;
				}
				
				index += 2;
			}
		}
	}
	
	if (destination != NULL) {
		destination[offset] = '\0';
	}
	
	offset++;
	
	return offset;
	
}
