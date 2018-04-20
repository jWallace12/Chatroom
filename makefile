all: prog3_server prog3_observer prog3_participant

prog3_server: prog3_server.c
	gcc -o prog3_server prog3_server.c
prog3_observer: prog3_observer.c
	gcc -o prog3_observer prog3_observer.c
prog3_participant: prog3_participant.c
	gcc -o prog3_participant prog3_participant.c
