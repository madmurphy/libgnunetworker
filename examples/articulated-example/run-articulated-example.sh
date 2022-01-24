#!/usr/bin/sh
#
# run-foobar.sh
#

gcc -pedantic -Wall -pthread -lgnunetutil -lgnunetworker -o '/tmp/articulated-example' gnunet-thread.c all-other-threads.c && \
	'/tmp/articulated-example' && rm '/tmp/articulated-example'
