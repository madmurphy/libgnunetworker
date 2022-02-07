#!/usr/bin/sh
#
# run-barriers-example.sh
#

gcc -pedantic -Wall -pthread -lgnunetworker -o '/tmp/barriers-example' barriers-example.c && \
	'/tmp/barriers-example' && rm '/tmp/barriers-example'
