#!/usr/bin/sh
#
# run-foobar.sh
#

gcc -pedantic -Wall -lgnunetutil -lgnunetworker -lgnunetfs `pkg-config --cflags gtk4` \
	-o '/tmp/foobar' foobar-gnunet.c foobar-ui.c foobar-main.c `pkg-config --libs gtk4` && \
	'/tmp/foobar' -c /etc/gnunet.conf && rm '/tmp/foobar'
