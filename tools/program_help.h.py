#!/usr/bin/env python3

import argparse
import os
import io
import json

parser = argparse.ArgumentParser(
	prog = "kad",
	description = "A simple HTTP proxy server that forwards all requests through curl-impersonate.",
	allow_abbrev = False,
	add_help = False,
	epilog = "Note, options that take a value must use an equal sign (e.g. --host=HOST)."
)

parser.add_argument(
	"-h",
	"--help",
	required = False,
	action = "store_true",
	help = "Display this help text and exit."
)

parser.add_argument(
	"-v",
	"--version",
	action = "store_true",
	help = "Display the Kad version and exit."
)

parser.add_argument(
	"--host",
	metavar = "HOST",
	required = False,
	help = "Bind socket to this host. [default: 127.0.0.1]"
)

parser.add_argument(
	"--port",
	metavar = "PORT",
	required = False,
	help = "Bind socket to this port. [default: 4000]"
)

parser.add_argument(
	"--target",
	metavar = "TARGET",
	required = False,
	help = "Impersonate this target. [default: chrome116]"
)

parser.add_argument(
	"--platform",
	metavar = "PLATFORM",
	required = False,
	help = "Impersonate this platform when the target is a generic Chrome, Edge or Firefox target (e.g. chrome131). Valid platforms: 'windows', 'macos', 'linux', 'android'."
)

parser.add_argument(
	"--loglevel",
	required = False,
	help = "Set output verbosity. Valid levels: 'quiet', 'standard', 'warning', 'error', 'info', 'verbose'. [default: verbose]"
)

parser.add_argument(
	"--proxy",
	metavar = "URL",
	required = False,
	help = "Forward all requests through this proxy server."
)

parser.add_argument(
	"--doh-url",
	metavar = "URL",
	required = False,
	help = "Use this DoH server for hostname resolution."
)

parser.add_argument(
	"--cacert",
	metavar = "FILE",
	required = False,
	help = "Overwrite the path to the CA certificate bundle to use."
)

parser.add_argument(
	"--dns-servers",
	metavar = "LIST",
	required = False,
	help = "Comma-separated list of DNS servers to use. [default: the embedded recursive resolver, falling back to 8.8.8.8,8.8.4.4]"
)

parser.add_argument(
	"--retry",
	metavar = "COUNT",
	required = False,
	help = "Retry idempotent requests whose transfer failed with a retryable error (send, receive or timeout failures) up to COUNT times, when no response bytes were delivered to the client yet. [default: 0]"
)

os.environ["LINES"] = "1000"
os.environ["COLUMNS"] = "1000"

file = io.StringIO()
parser.print_help(file = file)
file.seek(0, io.SEEK_SET)

text = file.read()

header = """/*
This file is auto-generated. Use the tool at ../tools/program_help.h.py to regenerate.
*/

#if !defined(PROGRAM_HELP_H)
#define PROGRAM_HELP_H

#define PROGRAM_HELP \\\n\
"""

for line in text.splitlines():
	line = json.dumps(obj = line + "\n")
	header += '\t%s\\\n' % line

header += "\n#endif\n"

destination = os.path.join(
	os.path.dirname(
		p = os.path.dirname(
			p = os.path.realpath(
				filename = __file__
			)
		)
	),
	"src/program_help.h"
)
	
print("Saving to '%s'" % (destination))

with open(file = destination, mode = "w") as file:
	file.write(header)
