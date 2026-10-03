#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && defined(_UNICODE)
	#include <windows.h>
#endif

#include "argparse.h"

static const arg_t* argparse_next(
	argparse_t* const parser,
	arg_t* const argument
) {
	
	const char* astart = NULL;
	const char* aend = NULL;
	
	const char* kstart = NULL;
	const char* kend = NULL;
	
	const char* vstart = NULL;
	const char* vend = NULL;
	
	size_t ksize = 0;
	size_t vsize = 0;
	
	char* item = NULL;
	
	int vonly = 0;
	
	#if defined(_WIN32) && defined(_UNICODE)
		const wchar_t* witem = NULL;
		int items = 0;
	#endif
	
	argument->key = NULL;
	argument->value = NULL;
	
	if (parser->index >= parser->argc) {
		return NULL;
	}
	
	#if defined(_WIN32) && defined(_UNICODE)
		witem = parser->argv[parser->index++];
		items = WideCharToMultiByte(CP_UTF8, 0, witem, -1, NULL, 0, NULL, NULL);
		
		if (items == 0) {
			return NULL;
		}
		
		item = malloc((size_t) items);
		
		if (item == NULL) {
			return NULL;
		}
		
		if (WideCharToMultiByte(CP_UTF8, 0, witem, -1, item, items, NULL, NULL) == 0) {
			free(item);
			return NULL;
		}
	#else
		item = parser->argv[parser->index++];
	#endif
	
	astart = item;
	aend = strchr(item, '\0');
	
	kstart = astart;
	kend = strstr(astart, "=");
	
	if (kend == NULL) {
		kend = aend;
	}
	
	argument->type = ARGTYPE_NON_VALUE;
	
	while (*kstart == '-') {
		kstart++;
	}
	
	vonly = (kstart == astart);
	
	if (!vonly) {
		ksize = (size_t) (kend - kstart);
		
		argument->key = malloc(ksize + 1);
		
		if (argument->key == NULL) {
			#if defined(_WIN32) && defined(_UNICODE)
				free(item);
			#endif
			
			return NULL;
		}
		
		memcpy(argument->key, kstart, ksize);
		argument->key[ksize] = '\0';
	}
	
	vstart = vonly ? kstart : kend;
	
	if (!vonly && vstart != aend) {
		vstart += 1;
	}
	
	vend = aend;
	
	vsize = (size_t) (vend - vstart);
	
	if (vsize > 0) {
		argument->value = malloc(vsize + 1);
		
		if (argument->value == NULL) {
			#if defined(_WIN32) && defined(_UNICODE)
				free(item);
			#endif
			
			free(argument->key);
			argument->key = NULL;
			
			return NULL;
		}
		
		memcpy(argument->value, vstart, vsize);
		argument->value[vsize] = '\0';
		
		argument->type = (vonly) ? ARGTYPE_VALUE_ONLY : ARGTYPE_VALUE;
	}
	
	#if defined(_WIN32) && defined(_UNICODE)
		free(item);
	#endif
	
	return argument;
	
}

int argparse_init(
	argparse_t* const parser,
	const int argc,
	argv_t** const argv
) {
	
	int err = KADERR_SUCCESS;
	
	size_t index = 0;
	size_t size = 0;
	
	arg_t argument = {0};
	
	arg_t* prev = NULL;
	arg_t* cur = NULL;
	
	parser->index = 1;
	parser->argc = (size_t) argc;
	parser->argv = argv;
	
	parser->arguments.offset = 0;
	
	if (parser->argc < 2) {
		return KADERR_SUCCESS;
	}
	
	parser->arguments.size = sizeof(argument) * (parser->argc - 1);
	parser->arguments.items = malloc(parser->arguments.size);
	
	if (parser->arguments.items == NULL) {
		err = KADERR_MEMORY_ALLOCATE_FAILURE;
		goto end;
	}
	
	while (argparse_next(parser, &argument) != NULL) {
		parser->arguments.items[parser->arguments.offset++] = argument;
		
		if (argument.key == NULL && argument.value == NULL) {
			err = KADERR_ARGPARSE_ARGUMENT_EMPTY;
			goto end;
		}
	}
	
	for (index = 0; index < parser->arguments.offset; index++) {
		cur = &parser->arguments.items[index];
		
		if (cur->type != ARGTYPE_VALUE_ONLY) {
			continue;
		}
		
		if (index == 0) {
			err = KADERR_ARGPARSE_VALUE_UNEXPECTED;
			goto end;
		}
		
		prev = &parser->arguments.items[index - 1];
		
		if (prev->type != ARGTYPE_NON_VALUE) {
			err = KADERR_ARGPARSE_VALUE_UNEXPECTED;
			goto end;
		}
		
		prev->type = ARGTYPE_VALUE;
		prev->value = cur->value;
		
		cur->value = NULL;
		size = (parser->arguments.offset - (index + 1)) * sizeof(argument);
		
		if (size > 0) {
			memmove(cur, cur + 1, size);
		}
		
		parser->arguments.offset--;
		index--;
	}
	
	end:;
	
	parser->index = 0;
	
	if (err != KADERR_SUCCESS) {
		argparse_free(parser);
	}
	
	return err;
	
}

const arg_t* argparse_getnext(argparse_t* const parser) {
	
	if (parser->index >= parser->arguments.offset) {
		return NULL;
	}
	
	return &parser->arguments.items[parser->index++];
	
}

static void argument_free(arg_t* const argument) {
	
	free(argument->key);
	argument->key = NULL;
	
	free(argument->value);
	argument->value = NULL;
	
}

void argparse_free(argparse_t* const parser) {
	
	size_t index = 0;
	
	arg_t* argument = NULL;
	
	parser->index = 0;
	parser->argc = 0;
	parser->argv = NULL;
	
	for (index = 0; index < parser->arguments.offset; index++) {
		argument = &parser->arguments.items[index];
		argument_free(argument);
	}
	
	parser->arguments.offset = 0;
	parser->arguments.size = 0;
	
	free(parser->arguments.items);
	parser->arguments.items = NULL;
	
}
