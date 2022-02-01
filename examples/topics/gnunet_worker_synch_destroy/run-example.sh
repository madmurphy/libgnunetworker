#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwsd' gwsd.c && \
	'/tmp/gwsd' && rm '/tmp/gwsd'
