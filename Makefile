keymap: main.c
	gcc -DNDEBUG main.c -o keymap -Wall

debug: main.c
	gcc main.c -o keymap -Wall -ggdb

install: keymap
	sudo cp keymap /usr/local/sbin/