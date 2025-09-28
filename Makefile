CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=gnu17
LDFLAGS =

TARGETS = peer-time-sync
.PHONY: all clean

all: $(TARGETS)

clean:
	$(RM) $(TARGETS) *.o

peer-time-sync: peer-time-sync.o protocol.o peers.o messages.o network.o common.o
	$(CC) $(LDFLAGS) -o $@ $^ -lm

messages.o: messages.c messages.h common.h
peer-time-sync.o: peer-time-sync.c protocol.h peers.h messages.h common.h
protocol.o: protocol.c protocol.h peers.h messages.h common.h
peers.o: peers.c peers.h common.h
network.o: network.c network.h common.h
common.o: common.c common.h
