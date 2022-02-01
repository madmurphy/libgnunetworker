#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwtd' gwtd.c && \
	'/tmp/gwtd' && rm '/tmp/gwtd'
