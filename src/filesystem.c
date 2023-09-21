#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
	#include <windows.h>
	#include <fileapi.h>
#endif

#if defined(__FreeBSD__)
	#include <sys/types.h>
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
	#include <sys/sysctl.h>
#endif

#if defined(__HAIKU__)
	#include <FindDirectory.h>
#endif

#if !defined(__HAIKU__)
	#include <fcntl.h>
#endif

#if !defined(_WIN32)
	#include <unistd.h>
	#include <limits.h>
#endif

#include "constants.h"
#include "filesystem.h"

char* get_app_filename(char* const filename) {
	/*
	Returns the filename of the application's executable.
	
	Returns a null pointer on error.
	*/
	
	#if defined(_WIN32)
		#if defined(_UNICODE)
			wchar_t wfilename[PATH_MAX];
			const DWORD code = GetModuleFileNameW(0, wfilename, (DWORD) (sizeof(wfilename) / sizeof(*wfilename)));
			
			if (code == 0 || code > (DWORD) (sizeof(wfilename) / sizeof(*wfilename))) {
				return NULL;
			}
			
			if (WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, filename, PATH_MAX, NULL, NULL) == 0) {
				return NULL;
			}
		#else
			if (GetModuleFileNameA(0, filename, PATH_MAX) == 0) {
				return NULL;
			}
		#endif
	#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#if defined(__NetBSD__)
			const int call[] = {
				CTL_KERN,
				KERN_PROC_ARGS,
				-1,
				KERN_PROC_PATHNAME
			};
		#else
			const int call[] = {
				CTL_KERN,
				KERN_PROC,
				KERN_PROC_PATHNAME,
				-1
			};
		#endif
		
		size_t size = PATH_MAX;
		
		if (sysctl(call, sizeof(call) / sizeof(*call), filename, &size, NULL, 0) == -1) {
			return NULL;
		}
	#elif defined(__OpenBSD__)
		const pid_t pid = getpid();
		
		const int call[] = {
			CTL_KERN,
			KERN_PROC_ARGS,
			pid,
			KERN_PROC_ARGV
		};
		
		size_t size = 0;
		
		if (sysctl(call, sizeof(call) / sizeof(*call), NULL, &size, NULL, 0) == -1) {
			return NULL;
		}
		
		char* argv[size / sizeof(char*)];
		
		if (sysctl(call, sizeof(call) / sizeof(*call), argv, &size, NULL, 0) == -1) {
			return NULL;
		}
		
		const char* const name = *argv;
		
		if (*name == *PATH_SEPARATOR) {
			// Path is absolute
			if (realpath(name, filename) == NULL) {
				return NULL;
			}
			
			return filename;
		}
		
		// Not an absolute path, check if it's relative to the current working directory
		int is_relative = 0;
		
		for (size_t index = 1; index < strlen(name); index++) {
			const char ch = name[index];
			
			is_relative = ch == *PATH_SEPARATOR;
			
			if (is_relative) {
				break;
			}
		}
		
		if (is_relative) {
			char cwd[PATH_MAX] = {'\0'};
			
			if (getcwd(cwd, sizeof(cwd)) == NULL) {
				return NULL;
			}
			
			char path[strlen(cwd) + strlen(PATH_SEPARATOR) + strlen(name) + 1];
			strcpy(path, cwd);
			strcat(path, PATH_SEPARATOR);
			strcat(path, name);
			
			if (realpath(path, filename) == NULL) {
				return NULL;
			}
			
			return filename;
		}
		
		// Search in PATH
		const char* const path = getenv("PATH");
		
		if (path == NULL) {
			return NULL;
		}
		
		const char* start = path;
		
		for (size_t index = 0; index < strlen(path) + 1; index++) {
			const char* const ch = &path[index];
			
			if (!(*ch == *COLON || *ch == '\0')) {
				continue;
			}
			
			const size_t size = (size_t) (ch - start);
			
			char executable[size + strlen(PATH_SEPARATOR) + strlen(name) + 1];
			memcpy(executable, start, size);
			executable[size] = '\0';
			
			strcat(executable, PATH_SEPARATOR);
			strcat(executable, name);
			
			switch (file_exists(executable)) {
				case 1: {
					if (realpath(executable, filename) == NULL) {
						return NULL;
					}
					
					return filename;
				}
				case -1: {
					return NULL;
				}
			}
			
			start = ch + 1;
		}
		
		errno = ENOENT;
		
		return NULL;
	#elif defined(__APPLE__)
		char path[PATH_MAX] = {'\0'};
		uint32_t paths = (uint32_t) sizeof(path);
		
		if (_NSGetExecutablePath(path, &paths) == -1) {
			return 0;
		}
		
		char resolved_path[PATH_MAX] = {'\0'};
		
		if (realpath(path, resolved_path) == NULL) {
			return NULL;
		}
		
		strcpy(filename, resolved_path);
	#elif defined(__HAIKU__)
		if (find_path(NULL, B_FIND_PATH_IMAGE_PATH, NULL, filename, PATH_MAX) != B_OK) {
			return NULL;
		}
	#else
		if (readlink("/proc/self/exe", filename, PATH_MAX) == -1) {
			return NULL;
		}
	#endif
	
	return filename;
	
}
