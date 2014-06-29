keymap: main.c
	gcc -O2 -DNDEBUG main.c -o keymap -Wall

debug: main.c
	gcc main.c -o keymap -Wall -ggdb

install: keymap
	sudo cp keymap /usr/local/sbin/