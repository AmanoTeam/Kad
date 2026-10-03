#if !defined(BUFFER_H)
#define BUFFER_H

#include <stdlib.h>

struct buffer_t {
	char* s;
	size_t slength;
};

typedef struct buffer_t buffer_t;

void buffer_free(buffer_t* const buffer);

#define __buffer_free__ __attribute__((__cleanup__(buffer_free)))

#endif
