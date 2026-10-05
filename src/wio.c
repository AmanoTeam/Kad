#include <stdio.h>
#include <stdarg.h>

#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include "wio.h"

int wio_printf(const char* const format, ...) {
	
	int wsize = 0;
	
	char* value = NULL;
	wchar_t* wvalue = NULL;
	
	va_list list;
	va_start(list, format);
	
	wsize = vsnprintf(NULL, 0, format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	value = malloc((size_t) (wsize + 1));
	
	if (value == NULL) {
		wsize = -1;
		goto end;
	}
	
	wsize = vsnprintf(value, (size_t) (wsize + 1), format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
	
	if (wsize == 0) {
		wsize = -1;
		goto end;
	}
	
	wvalue = malloc(((size_t) wsize) * sizeof(*wvalue));
	
	if (wvalue == NULL) {
		wsize = -1;
		goto end;
	}
	
	if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wsize) == 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = wprintf(L"%ls", wvalue);
	
	end:;
	
	free(value);
	free(wvalue);
	
	va_end(list);
	
	return wsize;
	
}

int wio_fprintf(FILE* const stream, const char* const format, ...) {
	
	int wsize = 0;
	
	char* value = NULL;
	wchar_t* wvalue = NULL;
	
	va_list list;
	va_start(list, format);
	
	wsize = vsnprintf(NULL, 0, format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	value = malloc((size_t) (wsize + 1));
	
	if (value == NULL) {
		wsize = -1;
		goto end;
	}
	
	wsize = vsnprintf(value, (size_t) (wsize + 1), format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
	
	if (wsize == 0) {
		wsize = -1;
		goto end;
	}
	
	wvalue = malloc(((size_t) wsize) * sizeof(*wvalue));
	
	if (wvalue == NULL) {
		wsize = -1;
		goto end;
	}
	
	if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wsize) == 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = fwprintf(stream, L"%ls", wvalue);
	
	end:;
	
	free(value);
	free(wvalue);
	
	va_end(list);
	
	return wsize;
	
}

int wio_vfprintf(FILE* const stream, const char* const format, va_list list) {
	
	int wsize = 0;
	
	char* value = NULL;
	wchar_t* wvalue = NULL;
	
	wsize = vsnprintf(NULL, 0, format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	value = malloc((size_t) (wsize + 1));
	
	if (value == NULL) {
		wsize = -1;
		goto end;
	}
	
	wsize = vsnprintf(value, (size_t) (wsize + 1), format, list);
	
	if (wsize < 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
	
	if (wsize == 0) {
		wsize = -1;
		goto end;
	}
	
	wvalue = malloc(((size_t) wsize) * sizeof(*wvalue));
	
	if (wvalue == NULL) {
		wsize = -1;
		goto end;
	}
	
	if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wvalue, wsize) == 0) {
		wsize = -1;
		goto end;
	}
	
	wsize = fwprintf(stream, L"%ls", wvalue);
	
	end:;
	
	free(value);
	free(wvalue);
	
	return wsize;
	
}

int wio_enable_unicode(void) {
	
	int fd = 0;
	int status = 0;
	
	fd = _fileno(stdout);
	
	if (fd == -1) {
		return -1;
	}
	
	status = _setmode(fd, _O_WTEXT);
	
	if (status == -1) {
		return -1;
	}
	
	fd = _fileno(stderr);
	
	if (fd == -1) {
		return -1;
	}
	
	status = _setmode(fd, _O_WTEXT);
	
	if (status == -1) {
		return -1;
	}
	
	fd = _fileno(stdin);
	
	if (fd == -1) {
		return -1;
	}
	
	status = _setmode(fd, _O_WTEXT);
	
	if (status == -1) {
		return -1;
	}
	
	return 0;
	
}
