#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwgch' gwgch.c && \
	'/tmp/gwgch' && rm '/tmp/gwgch'
