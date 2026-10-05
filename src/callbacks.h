#if !defined(CALLBACKS_H)
#define CALLBACKS_H

#include <stdlib.h>

int sock_write(void* fd, const unsigned char* buffer, size_t buffer_size);
int sock_read(void* fd, unsigned char* buffer, size_t buffer_size);

size_t read_callback(char* dest, size_t size, size_t nmemb, void* userp);
size_t read_callback_empty(char* dest, size_t size, size_t nmemb, void* userp);
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userp);
size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);

#endif
