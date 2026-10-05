#include <stdlib.h>

#include "buffer.h"

void buffer_free(buffer_t* const buffer) {
	
	free(buffer->s);
	buffer->s = NULL;
	buffer->slength = 0;
	
}
