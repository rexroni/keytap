keytap: main.c
	gcc -O2 -DNDEBUG main.c -o keytap -Wall

debug: main.c
	gcc main.c -o keytap -Wall -ggdb

install: keytap
	sudo chown root:root keytap
	sudo chmod 4755 keytap
	sudo cp -p keytap /usr/local/sbin/
