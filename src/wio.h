#if !defined(WIO_H)
#define WIO_H

#include <stdio.h>
#include <stdarg.h>

int wio_printf(const char* const format, ...);
int wio_fprintf(FILE* const stream, const char* const format, ...);
int wio_vfprintf(FILE* const stream, const char* const format, va_list list);

int wio_enable_unicode(void);

#define printf wio_printf
#define fprintf wio_fprintf
#define vfprintf wio_vfprintf

#endif
