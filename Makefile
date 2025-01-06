#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

CFLAGS = -c -O2 -Wall -fPIC
#thats cursed...
DBUS_CFLAGS = $(shell pkg-config --cflags dbus-1)
DBUS_LIBS = $(shell pkg-config --libs dbus-1)

all: MusicBot

MusicBot: plugin.o
	gcc -o MusicBot.so -shared plugin.o $(DBUS_LIBS)

plugin.o: ./src/plugin.c
	gcc -Iinclude src/plugin.c $(CFLAGS) $(DBUS_CFLAGS) -o plugin.o

clean:
	rm -rf *.o MusicBot.so
