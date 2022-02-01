#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwss' gwss.c && \
	'/tmp/gwss' && rm '/tmp/gwss'
