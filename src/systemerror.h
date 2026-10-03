#if !defined(SYSTEMERROR_H)
#define SYSTEMERROR_H

struct SystemError {
	int code;
	char message[256];
};

typedef struct SystemError system_error_t;

system_error_t get_system_error(void);

#endif
