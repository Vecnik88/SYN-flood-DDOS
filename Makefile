all: clean syn-flood

CC = gcc
INC = ./inc
SRC = ./src
CFLAGS =-g -Wall -g3 -O0 -I$(INC)
LDFLAGS = -pthread
SOURCES = $(wildcard ./src/*.c)
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = syn-flood
INSTPATH = /usr/bin

.PHONY: all clean run install uninstall

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	-rm -f ./src/*.o $(EXECUTABLE)

install:
	sudo install $(EXECUTABLE) $(INSTPATH)

uninstall:
	sudo rm $(INSTPATH)/$(EXECUTABLE)

run:
	$(INSTPATH)/$(EXECUTABLE)