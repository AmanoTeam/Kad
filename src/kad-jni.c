#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "kad.h"

JNIEXPORT jint JNICALL Java_com_amanoteam_kad_Kad_kadMain(JNIEnv* env, jclass caller, jobjectArray arguments) {
	
	static const char KAD_JNI_PROGRAM_NAME[] = "kad";
	
	jsize count = 0;
	jsize index = 0;
	
	size_t size = 0;
	
	char** argv = NULL;
	const char* utf = NULL;
	
	jstring item = NULL;
	
	jint code = 0;
	
	(void) caller;
	
	if (arguments == NULL) {
		return -1;
	}
	
	count = (*env)->GetArrayLength(env, arguments);
	
	argv = calloc((size_t) (count + 1), sizeof(char*));
	
	if (argv == NULL) {
		return -1;
	}
	
	argv[0] = (char*) KAD_JNI_PROGRAM_NAME;
	
	while (index < count) {
		item = (jstring) (*env)->GetObjectArrayElement(env, arguments, index);
		
		utf = (*env)->GetStringUTFChars(env, item, NULL);
		
		if (utf == NULL) {
			code = -1;
			goto end;
		}
		
		size = strlen(utf);
		
		argv[index + 1] = malloc(size + 1);
		
		if (argv[index + 1] == NULL) {
			(*env)->ReleaseStringUTFChars(env, item, utf);
			
			code = -1;
			goto end;
		}
		
		memcpy(argv[index + 1], utf, size + 1);
		
		(*env)->ReleaseStringUTFChars(env, item, utf);
		
		index++;
	}
	
	code = (jint) kad_main((int) (count + 1), argv);
	
	end:;
	
	while (index > 0) {
		free(argv[index]);
		index--;
	}
	
	free(argv);
	
	return code;
	
}
