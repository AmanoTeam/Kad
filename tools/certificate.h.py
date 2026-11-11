#!/usr/bin/env python3

import subprocess

CERTIFICATE_COMMAND = [
	"openssl",
	"x509",
	"-in",
	"./certificates/kad.crt",
	"-outform",
	"DER"
]

PRIVATE_KEY_COMMAND = [
	"openssl",
	"rsa",
	"-in",
	"./certificates/kad.key",
	"-outform",
	"DER",
	"-traditional"
]

WIDTH = 12

TEMPLATE = """/*
This file is auto-generated. Use the tool at ../tools/certificate.h.py to regenerate.
*/

#pragma once

// TLS certificate (DER)

%s

// RSA private key (DER, PKCS #1)

%s
"""

def array(name, data):
	entries = ["0x%02X" % (byte) for byte in data]

	lines = [
		"\t" + ", ".join(entries[offset:offset + WIDTH]) + "," * (offset + WIDTH < len(entries))
		for offset in range(0, len(entries), WIDTH)
	]

	return "static const unsigned char %s[] = {\n%s\n};" % (
		name,
		"\n".join(lines)
	)

certificate = subprocess.check_output(
	CERTIFICATE_COMMAND
)

private_key = subprocess.check_output(
	PRIVATE_KEY_COMMAND
)

header = TEMPLATE % (
	array(name = "CERTIFICATE", data = certificate),
	array(name = "RSA_PRIVATE_KEY", data = private_key)
)

with open(file = "../src/certificate.h", mode = "w") as file:
	file.write(header)

print("Saving to '../src/certificate.h'")
