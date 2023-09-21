#!/usr/bin/env python3

import subprocess

header = "#include <bearssl.h>\n\n// {} {}\n#pragma once\n"

output = subprocess.check_output(
	[
		"brssl",
		"chain",
		"-q",
		"./certificates/kad.crt"
	]
)

certificate = output.decode()

output = subprocess.check_output(
	[
		"brssl",
		"skey",
		"-q",
		 "-C",
		"./certificates/kad.key"
	]
)

key = output.decode()

content = header.format(key, certificate)

with open(file = "../src/certificate.h", mode = "w") as file:
	file.write(content)
