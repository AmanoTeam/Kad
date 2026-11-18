#if !defined(FSTREAM_H)
#define FSTREAM_H

#include <stdlib.h>
#include <stdint.h>

#if defined(_WIN32)
	#include <windows.h>
#endif

#if !defined(_WIN32)
	#include <stdio.h>
#endif

#define FSTREAM_SUCCESS (0)
#define FSTREAM_ERROR (-1)
#define FSTREAM_EOF (0)

enum FStreamMode {
	FSTREAM_WRITE,
	FSTREAM_READ,
	FSTREAM_APPEND,
	FSTREAM_TRUNCATE
};

typedef enum FStreamMode fstream_mode_t;

struct FStream {
	fstream_mode_t mode;
#if defined(_WIN32)
	HANDLE stream;
#else
	FILE* stream;
#endif
};

typedef struct FStream fstream_t;

fstream_t* fstream_open(const char* const filename, const fstream_mode_t mode);
int fstream_write(fstream_t* const stream, const char* const buffer, const size_t size);
int64_t fstream_size(fstream_t* const stream);
int fstream_close(fstream_t* const stream);

#endif
