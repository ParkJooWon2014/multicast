#include "types.h"

#ifndef __IB_H__
#define __IB_H__

int post_recv(struct node *node);
int post_send(struct node *node ,void * buffer, size_t size);
int get_completion(struct node * n, bool type);

#endif
