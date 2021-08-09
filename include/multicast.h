
#ifndef __MC_H__
#define __MC_H__

#include "types.h"
void die(const char * reason);
int process_rdma_cm_event(struct rdma_event_channel *echannel, 
		enum rdma_cm_event_type expected_event,
		struct rdma_cm_event **cm_event);
struct ctrl* alloc_ctrl(void);
struct node *rdma_create_node(struct ctrl * ctrl);
struct node * alloc_node(struct ctrl * ctrl);
int resolve_addr(struct ctrl *ctrl);
void destory_ctrl(struct ctrl * ctrl);
#endif
