#!/usr/bin/sh
#
# run-simple-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/simple-example' simple-example.c && \
	'/tmp/simple-example' && rm '/tmp/simple-example'
