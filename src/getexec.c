#include <stdlib.h>
#include <string.h>

#include <whereami.h>

#include "getexec.h"

char* get_app_filename(void) {
	/*
	Returns the filename of the application's executable.

	Returns a null pointer on error.
	*/

	int length = 0;
	int dirname_length = 0;

	char* app_filename = NULL;

	length = wai_getExecutablePath(NULL, 0, &dirname_length);

	if (length < 0) {
		return NULL;
	}

	app_filename = malloc((size_t) length + 1);

	if (app_filename == NULL) {
		return NULL;
	}

	if (wai_getExecutablePath(app_filename, length, &dirname_length) == -1) {
		free(app_filename);

		return NULL;
	}

	app_filename[length] = '\0';

	return app_filename;

}

char* get_app_directory(void) {

	int length = 0;
	int dirname_length = 0;
	int offset = 0;

	size_t namesize = 0;

	char* app_filename = NULL;
	char* app_directory = NULL;

	const char* name = NULL;

	length = wai_getExecutablePath(NULL, 0, &dirname_length);

	if (length < 0) {
		return NULL;
	}

	app_filename = malloc((size_t) length + 1);

	if (app_filename == NULL) {
		return NULL;
	}

	if (wai_getExecutablePath(app_filename, length, &dirname_length) == -1) {
		free(app_filename);

		return NULL;
	}

	app_filename[length] = '\0';

	app_directory = malloc((size_t) dirname_length + 2);

	if (app_directory == NULL) {
		free(app_filename);

		return NULL;
	}

	memcpy(app_directory, app_filename, (size_t) dirname_length);

	app_directory[dirname_length] = '\0';

	free(app_filename);

	name = app_directory + dirname_length;

	while (name > app_directory && name[-1] != '/' && name[-1] != '\\') {
		name--;
	}

	namesize = strlen(name);

	if (!(
		(namesize == 4 && (name[0] == 'x' || name[0] == 's') && strcmp(name + 1, "bin") == 0) ||
		strcmp(name, "bin") == 0
	)) {
		return app_directory;
	}

	offset = (int) (name - app_directory) - 1;

	if (offset > 0) {
		app_directory[offset] = '\0';
	} else if (offset == 0) {
		app_directory[0] = '/';
		app_directory[1] = '\0';
	} else {
		app_directory[0] = '\0';
	}

	return app_directory;

}
