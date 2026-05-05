CC ?= gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?=
TARGET ?= aesdsocket

all: $(TARGET)
default: $(TARGET)

$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.o

.PHONY: all default clean
