# Makefile for the hw0
# write by Haoyu Chen
CC=gcc
CFLAGS=-c -g --std=c99 

all: wc main map

wc: wc.o
	$(CC) wc.o -o wc

main: main.o
	$(CC) main.o -o main

map: map.o
	$(CC) map.o -o map

wc.o: wc.c
	$(CC) $(CFLAGS) wc.c

main.o: main.c
	$(CC) $(CFLAGS) main.c

map.o: map.c
	$(CC) $(CFLAGS) map.c

clean:
	rm *o wc main map
