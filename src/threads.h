#if !defined(THREADS_H)
#define THREADS_H

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
	
	typedef struct platform_thread_data thread_data_t;
	
	DWORD WINAPI thread_callback(LPVOID lpParameter);
	
	typedef SRWLOCK mutex_t;
	
	#define MUTEX_INIT SRWLOCK_INIT
#else
	struct platform_thread {
		pthread_t thread;
	};
	
	typedef pthread_mutex_t mutex_t;
	
	#define MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#endif

typedef struct platform_thread thread_t;

int thread_create(thread_t* const thread, void*(*callback)(void*), void* const argument);
int thread_wait(thread_t* const thread);

void mutex_lock(mutex_t* const mutex);
void mutex_unlock(mutex_t* const mutex);

#endif
