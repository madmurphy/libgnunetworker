#!/usr/bin/sh
#
# run-example.sh
#

gcc -pedantic -Wall -lgnunetworker -o '/tmp/gwgd' gwgd.c && \
	'/tmp/gwgd' && rm '/tmp/gwgd'
