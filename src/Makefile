CC		= gcc

INCLUDE = -I../include
LIBMC = libmc.a

CFLAGS += $(INCLUDE)
CFLAGS += -D_POSIX_C_SOURCE -D_GNU_SOURCE
CFLAGS += -g 
CFLAGS += -Wall -Werror

LDFLAGS += -L../lib
LDFLAGS += -lpthread -libverbs 
#-l:$(LIBMC)"

TARGETS = mctest 
all: $(TARGETS)

mctest: mctest.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f *.o $(TARGETS)

.PHONY: cscope
cscope:
	cscope -b -R
