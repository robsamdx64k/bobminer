CC ?= cc
CFLAGS ?= -O3 -Wall -Wextra -pthread
LDFLAGS ?= -lssl -lcrypto -ljansson -pthread

all: bobminer

bobminer: src/bobminer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f bobminer

.PHONY: all clean
