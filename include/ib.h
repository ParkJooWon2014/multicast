
#ifndef __IB_H__
#define __IB_H__


#include "types.h"

int mpost_recv(struct node *node);
int mpost_send(struct node *node ,void * buffer, size_t size);
int get_completion(struct node * n, bool type);

#endif
