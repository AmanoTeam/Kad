#include <stdlib.h>

#if defined(_WIN32)
	#include <winsock2.h>
#else
	#include <unistd.h>
#endif

#include "socketclose.h"

int socket_close(const int fd) {
	
	#if defined(_WIN32)
		return (closesocket((SOCKET) fd) == 0) ? 0 : -1;
	#else
		return close(fd);
	#endif
	
}
