#!/bin/bash

openssl req -x509 \
	-out './certificates/kad.crt' \
	-keyout './certificates/kad.key' \
	-newkey 'rsa:2048' \
	-nodes -sha256 \
	-days '1126' \
	-subj '/CN=Kad Root CA/O=Kad/OU=Kad' \
	-extensions 'EXT' \
	-config './ssl.conf'
