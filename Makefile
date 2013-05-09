# vim: ft=make ff=unix fenc=utf-8
# file: Makefile
LIBS=
CFLAGS=-Wall -Wextra -pedantic -std=c99

all: unetd

unetd: unetd.c
	${CC} -o $@ ${CFLAGS} ${LIBS} $<

install: unetd init.d/unetd
	install -m555 -s unetd /usr/bin
	install -m555 init.d/unetd /etc/init.d

uninstall: install
	rm -f /etc/init.d/unetd
	rm -f /usr/bin/unetd

