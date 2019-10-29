CC=gcc
CFLAGS=-Wall -Wno-unused-result
LD=ld
LDFLAGS=-llua -lsystemd

ifeq ($(DEBUG),true)
	CFLAGS+=-g
else
	CFLAGS+=-O2
endif

all: sdiol

sdiol: sdiol.o server.o resolver.o time_util.o networking.o devices.o config.o names.o

%.o: %.c %.h Makefile

.PHONY: clean
clean:
	rm -f *.o sdiol

.PHONY: debug
debug:
	DEBUG=true $(MAKE) all

install: sdiol
	install --owner root --group root --mode 4755 sdiol /usr/local/bin

install-systemd: install
	cp sdiol.service /etc/systemd/system/
