#if defined(_WIN32)
	#include <stdio.h>
#endif

#if !defined(_WIN32)
	#include <sys/resource.h>
#endif

#include "resources.h"

int resources_increase_maxfd(void) {
	
	int status = 0;
	
	#if defined(_WIN32)
		status = _setmaxstdio(2048);
		
		if (status == -1) {
			return -1;
		}
	#else
		struct rlimit rlim = {0};
		status = getrlimit(RLIMIT_NOFILE, &rlim);
		
		if (status == -1) {
			return status;
		}
		
		if (rlim.rlim_cur == rlim.rlim_max) {
			return 0;
		}
		
		rlim.rlim_cur = rlim.rlim_max;
		status = setrlimit(RLIMIT_NOFILE, &rlim);
		
		if (status == -1) {
			return status;
		}
	#endif
	
	return 0;
	
}
