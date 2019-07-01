.PHONY: debug

keytap: main.c
	gcc -Wall -Wno-unused-result -O2 -DNDEBUG main.c -o keytap

debug: main.c
	gcc -Wall -ggdb main.c -o keytap

install: keytap
	sudo chown root:root keytap
	sudo chmod 4755 keytap
	sudo cp -p keytap /usr/local/sbin/

install-systemd: install
	sudo cp -p keytap.service /etc/systemd/system/
