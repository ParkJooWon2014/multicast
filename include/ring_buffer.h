#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "types.h"


struct ring_buffer {

	void * buffer;
	uint64_t head;
	uint64_t tail;
	bool arounded;

};

void rb_init(struct ring_buffer *rb ,void *buffer);
int rb_full(struct ring_buffer *rb);
int rb_empty(struct ring_buffer *rb);
int rb_put(struct ring_buffer * rb, ,void * buffer,
		size_t size);
uint64_t get_rb_size(struct ring_buffer *rb);
void *rb_get(struct ring_buffer* rb, size_t *size);

