CC=gcc
CFLAGS=-Wall -Wno-unused-result
LD=ld

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


#
# keytap: main.c
# 	gcc -Wall -Wno-unused-result -O2 -DNDEBUG main.c -o keytap
#
# debug: main.c
# 	gcc -Wall -ggdb main.c -o keytap
#
# install: keytap
# 	sudo chown root:root keytap
# 	sudo chmod 4755 keytap
# 	sudo cp -p keytap /usr/local/sbin/
#
# install-systemd: install
# 	sudo cp -p keytap.service /etc/systemd/system/
