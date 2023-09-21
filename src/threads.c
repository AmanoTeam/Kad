#include <stdlib.h>

#if defined(_WIN32)
	#include <windows.h>
#else
	#include <pthread.h>
#endif

#include "threads.h"

#if defined(_WIN32)
	DWORD WINAPI thread_callback(LPVOID lpParameter) {
		
		thread_data_t* data = (thread_data_t*) lpParameter;
		(*data->callback)(data->argument);
		
		return 0;
		
	}
#endif

int thread_create(thread_t* const thread, void*(*callback)(void*), void* const argument) {
	
	#if defined(_WIN32)
		thread_data_t* data = malloc(sizeof(thread_data_t));
		
		if (data == NULL) {
			return -1;
		}
		
		data->callback = callback;
		data->argument = argument;
		
		thread->handle = CreateThread(NULL, 0, thread_callback, (void*) data, 0, &thread->id);
		
		if (thread->handle == NULL) {
			return -1;
		}
	#else
		if (pthread_create(&thread->thread, NULL, callback, argument) != 0) {
			return -1;
		}
	#endif
	
	return 0;
	
}

int thread_wait(thread_t* const thread) {
	
	#if defined(_WIN32)
		if (WaitForSingleObject(thread->handle, INFINITE) == WAIT_FAILED) {
			return -1;
		}
	#else
		if (pthread_join(thread->thread, NULL) != 0) {
			return -1;
		}
	#endif
	
	return 0;
	
	
}