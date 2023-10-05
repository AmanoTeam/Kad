#!/usr/bin/env python3

import argparse
import os
import io

parser = argparse.ArgumentParser(
    prog = "kad",
    description = None,
    allow_abbrev = False,
    add_help = False,
    epilog = "Note, options that take an argument require a equal sign. E.g. --host=HOST"
)

parser.add_argument(
	"-h",
	"--help",
	required = False,
	action = "store_true",
	help = "Show this help message and exit."
)

parser.add_argument(
	"-v",
    "--version",
    action = "store_true",
    help = "Display the Kad version and exit."
)

parser.add_argument(
    "--host",
    required = False,
    help = "Bind socket to this host. [default: 127.0.0.1]"
)

parser.add_argument(
    "--port",
    required = False,
    help = "Bind socket to this port. [default: 4000]"
)

parser.add_argument(
    "--target",
    required = False,
    help = "Impersonate this target. [default: chrome116]"
)

os.environ["LINES"] = "1000"
os.environ["COLUMNS"] = "1000"

file = io.StringIO()
parser.print_help(file = file)
file.seek(0, io.SEEK_SET)

text = file.read()

header = "/*\nThis file is auto-generated. Use the ../tools/program_help.h.py tool to regenerate.\n*/\n\n#define PROGRAM_HELP \\\n"

for line in text.splitlines():
	header += '\t"%s\\n" \\\n' % line

header += "\n#pragma once\n"

print("Saving to ../src/program_help.h")

with open(file = "../src/program_help.h", mode = "w") as file:
	file.write(header)
