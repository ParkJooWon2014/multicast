#include "ring_buffer.h"

struct *ring_buffer  rb_init(void *buffer)
{
	struct ring_bufer *rb = (struct ring_buffer *)malloc(sizeof(*rb));
	TEST_Z(rb);
	rb->tail = rb->head = (uint64_t)buffer;
	rb->arounded = false;

	return rb;
}

int rb_full(struct ring_buffer *rb)
{

	if(rb->arounded){
		if(rb->head == rb->tail)
			return 1;
	}else{
		if(rb->tail - rb->head == BUFFER_SIZE)
			return 1;
	}
	return 0;
}

int rb_empty(struct ring_buffer *rb)
{
	if(rb->arounded){
		if(rb->head - rb->tail == BUFFER_SIZE)
			return 1;
	}
	else {
		if(rb->head == rb->tail) 
			return 1;
	}
	return 0;
}

int rb_put(struct ring_buffer *rb, void * buffer, 
		size_t size)
{
                                                                                                                                                                                                           
}

void* rb_get(struct ring_buffer *rb)
