#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetutil -lgnunetworker -o '/tmp/gwd' gwd.c && \
	'/tmp/gwd' && rm '/tmp/gwd'
