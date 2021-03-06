
#ifndef __MC_H__
#define __MC_H__

#include "types.h"

int on_disconnect(struct node* n);
int on_multicast(struct node * node);
int on_event(struct rdma_cm_event *event);
struct ctrl *alloc_control(void);
struct node *alloc_node(struct ctrl *ctrl);
struct ctrl *get_ctrl(void);
struct devie * get_device(struct node *node);
int get_completion(struct node * n, bool type);
struct ctrl*  gen_control(struct rdma_cm_id * id ,struct rdma_conn_param * param);
struct ctrl*  rdma_client_create(struct rdma_cm_id *id, struct sockaddr *src_addr,
		struct sockaddr *dst_addr);
void destroy_device(struct ctrl *ctrl);
void die(const char * reason);
int process_rdma_cm_event(struct rdma_event_channel *echannel, 
		enum rdma_cm_event_type expected_event,
		struct rdma_cm_event **cm_event);
struct ctrl* alloc_ctrl(bool type);
struct node * rdma_create_node(struct rdma_cm_id * id , struct ctrl *ctrl);
int rdma_multi_solve(struct rdma_cm_id * id , struct ctrl *ctrl);
int get_addrinfo(struct ctrl*ctrl , struct sock_addr *sock,bool type);
#endif
