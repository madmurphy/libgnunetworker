#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwc' gwc.c && \
	'/tmp/gwc' && rm '/tmp/gwc'
