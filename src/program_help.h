/*
This file is auto-generated. Use the tool at ../tools/program_help.h.py to regenerate.
*/

#if !defined(PROGRAM_HELP_H)
#define PROGRAM_HELP_H

#define PROGRAM_HELP \
	"usage: kad [-h] [-v] [--host HOST] [--port PORT] [--target TARGET] [--platform PLATFORM] [--loglevel LOGLEVEL] [--proxy URL] [--doh-url URL] [--cacert FILE] [--dns-servers LIST] [--retry COUNT]\n"\
	"\n"\
	"A simple HTTP proxy server that forwards all requests through curl-impersonate.\n"\
	"\n"\
	"options:\n"\
	"  -h, --help           Display this help text and exit.\n"\
	"  -v, --version        Display the Kad version and exit.\n"\
	"  --host HOST          Bind socket to this host. [default: 127.0.0.1]\n"\
	"  --port PORT          Bind socket to this port. [default: 4000]\n"\
	"  --target TARGET      Impersonate this target. [default: chrome116]\n"\
	"  --platform PLATFORM  Impersonate this platform when the target is a generic Chrome, Edge or Firefox target (e.g. chrome131). Valid platforms: 'windows', 'macos', 'linux', 'android'.\n"\
	"  --loglevel LOGLEVEL  Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]\n"\
	"  --proxy URL          Forward all requests through this proxy server.\n"\
	"  --doh-url URL        Use this DoH server for hostname resolution.\n"\
	"  --cacert FILE        Overwrite the path to the CA certificate bundle to use.\n"\
	"  --dns-servers LIST   Comma-separated list of DNS servers to use. [default: the embedded recursive resolver, falling back to 8.8.8.8,8.8.4.4]\n"\
	"  --retry COUNT        Retry idempotent requests whose transfer failed with a retryable error (send, receive or timeout failures) up to COUNT times, when no response bytes were delivered to the client yet. [default: 0]\n"\
	"\n"\
	"Note, options that take a value must use an equal sign (e.g. --host=HOST).\n"\

#endif
