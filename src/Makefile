CC		= gcc

INCLUDE = -I../include
LIBKRONO = libmc.a

CFLAGS += $(INCLUDE)
CFLAGS += -D_POSIX_C_SOURCE -D_GNU_SOURCE
CFLAGS += -g 
CFLAGS += -Wall -Werror -Winline

LDFLAGS += -L../lib
LDFLAGS += -lpthread -libverbs -lrdmacm -l:$(LIBKRONO)

TARGETS =  client
all: $(TARGETS)

#server : server.o
#	$(CC) -o $@ $^ $(LDFLAGS)

client : client.o
	$(CC) -o $@ $^ $(LDFLAGS)

#mig1: mig1.o
#	$(CC) -o $@ $^ $(LDFLAGS) -static

#test: test.o
#	$(CC) -o $@ $^ $(LDFLAGS)

#test2: test2.o
#	$(CC) -o $@ $^ $(LDFLAGS)

#test3 : test3.o
#	$(CC) -o $@ $^ $(LDFLAGS)

#tester: tester
#	./tester

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGETS)

.PHONY: cscope
cscope:
	cscope -b -R
