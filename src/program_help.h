/*
This file is auto-generated. Use the tool at ../tools/program_help.h.py to regenerate.
*/

#if !defined(PROGRAM_HELP_H)
#define PROGRAM_HELP_H

#define PROGRAM_HELP \
	"usage: kad [-h] [-v] [--host HOST] [--port PORT] [--target TARGET] [--loglevel LOGLEVEL]\n"\
	"\n"\
	"A simple HTTP proxy server that forwards all requests through curl-impersonate.\n"\
	"\n"\
	"options:\n"\
	"  -h, --help           Display this help text and exit.\n"\
	"  -v, --version        Display the Kad version and exit.\n"\
	"  --host HOST          Bind socket to this host. [default: 127.0.0.1]\n"\
	"  --port PORT          Bind socket to this port. [default: 4000]\n"\
	"  --target TARGET      Impersonate this target. [default: chrome116]\n"\
	"  --loglevel LOGLEVEL  Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]\n"\
	"\n"\
	"Note, options that take a value must use an equal sign (e.g. --host=HOST).\n"\

#endif
