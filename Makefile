CC=gcc
CFLAGS=-Wall -Wno-unused-result
LD=ld
LDFLAGS=-llua -lsystemd

ifeq ($(DEBUG),true)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

all: keytap

keytap: keytap.o server.o resolver.o time_util.o networking.o devices.o config.o

%.o: %.c %.h Makefile

.PHONY: clean
clean:
	rm -f *.o keytap

.PHONY: debug
debug:
	DEBUG=true $(MAKE) all

install: keytap
	install --owner root --group root --mode 4755 keytap /usr/local/sbin

install-systemd: install
	cp keytap.service /etc/systemd/system/
