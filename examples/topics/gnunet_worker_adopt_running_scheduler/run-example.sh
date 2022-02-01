#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -lgnunetutil -pthread -o '/tmp/gwars' gwars.c && \
	'/tmp/gwars' && rm '/tmp/gwars'
