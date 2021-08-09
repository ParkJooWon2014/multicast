
LIBMC = libmc.a

CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

OFLAGS =
# Features
CFLAGS += $(OFLAGS)
CFLAGS += -I../include
CFLAGS += -D_POSIX_C_SOURCE -D_GNU_SOURCE -D_ISOC99_SOURCE
CFLAGS += -g -fPIC
CFLAGS += -Wall -Werror -Winline

CFLAGS += -lpthread -libverbs -lrdmacm

TARGETS = $(LIBMC)

all: $(TARGETS)

$(LIBMC): multicast.o  ib.o
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	rm -f $(TARGETS) *.o cscope.out

.PHONY: cscope
cscope:
	cscope -b -R
