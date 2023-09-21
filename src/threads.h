#if defined(_WIN32)
	#include <windows.h>
#else
	#include <pthread.h>
#endif

#if defined(_WIN32)
	struct platform_thread {
		DWORD id;
		HANDLE handle;
	};
	
	struct platform_thread_data {
		void*(*callback)(void*);
		void* argument;
	};
	
	DWORD WINAPI thread_callback(LPVOID lpParameter);
	
	typedef struct platform_thread_data thread_data_t;
#else
	struct platform_thread {
		pthread_t thread;
	};
#endif

typedef struct platform_thread thread_t;

int thread_create(thread_t* const thread, void*(*callback)(void*), void* const argument);
int thread_wait(thread_t* const thread);

#pragma once
