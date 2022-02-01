#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwad' gwad.c && \
	'/tmp/gwad' && rm '/tmp/gwad'
