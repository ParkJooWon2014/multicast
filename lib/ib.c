#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <fcntl.h> 
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_verbs.h>
#include <rdma/rdma_cma.h>
#include <pthread.h>

#include "types.h"
#include "multicast.h"

int post_recv(struct node *node);
int post_send(struct node *node ,void * buffer, size_t size);
int get_completion(struct node * n, bool type);


void*  __post_recv(void * _node)
{
	struct node *node = (struct node*)_node;
	TEST_Z(node);
	void * buffer = NULL;
	int ret = 0;
	while(node->state != INIT){

		TEST_NZ(ibv_req_notify_cq(node->rcv_cq,0));

		struct ibv_recv_wr wr = {};
		struct ibv_recv_wr *bad_wr;
		struct ibv_sge sge = {};

		wr.sg_list = &sge;
		wr.num_sge = 1 ;
		wr.next = NULL;

		sge.addr = (uint64_t)node->mr->addr;
		sge.length = BUFFER_SIZE;
		sge.lkey= node->mr->lkey;

		TEST_NZ(ibv_post_recv(node->qp, &wr,&bad_wr));
		ibv_get_cq_event(node->rcv_cc,&node->rcv_cq,&buffer);
		ret = get_completion(node,RECV);
		if(!ret){
			debug("----recv memory----");
			debug("%s\n",(char*)node->buffer);
			debug("-------------------");
		}
		ibv_ack_cq_events(node->rcv_cq, 1);
	}
	
	return NULL;
}
void *print(void * a)
{
	debug("SSSSIBA:LLLLL\n");
	return NULL;
}

int post_recv(struct node *node)
{
	pthread_t recv_thread;
	pthread_attr_t attr;
	 pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);	
	return pthread_create(&recv_thread,&attr,__post_recv,(void*)node);

}

static int __post_send(struct node* node, int type , void * buffer, size_t size)
{

	//struct device * dev = node->ctrl->dev;

	struct ibv_send_wr wr = {};
	struct ibv_send_wr *bad_wr = NULL;
	struct ibv_sge sge = {};
	struct ibv_mr * mr = NULL;

	TEST_Z(mr = rdma_reg_msgs(node->id,buffer,size)); /*ibv_reg_mr(
				dev->pd,
				buffer,
				size,
				IBV_ACCESS_LOCAL_WRITE)); // | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ));
*/
	debug("MR key=%u base vaddr=%p\n", mr->rkey, mr->addr);
	wr.wr_id = (uint64_t)buffer;
	wr.opcode = type; // IBV_WR_SEND;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.send_flags = IBV_SEND_SIGNALED;  // IBV_SEND_INLINE; //IBV_SEND_SIGNALED | IBV_SEND_INLINE;
	wr.wr.ud.ah = node->ah ;
	wr.wr.ud.remote_qpn = node->remote_qpn;
	wr.wr.ud.remote_qkey = node->remote_qkey;


	sge.addr = (uint64_t)buffer;
	sge.length = size;
	sge.lkey = mr->lkey;
	debug("buffer pointer is %lx\n",sge.addr);

	if(!node->qp)
		debug("I don't want \n");
	
	TEST_NZ(ibv_post_send(node->qp, &wr, &bad_wr));
	
	get_completion(node,SEND);
	
	return ibv_dereg_mr(mr);

}

int post_send(struct node *node ,void * buffer, size_t size)
{
	return  __post_send(node,IBV_WR_SEND,buffer,size);
}

int get_completion(struct node * n, bool type)
{
	int ret;
	struct ibv_wc wc;

	do {
		if(type == SEND)
			ret = ibv_poll_cq(n->snd_cq, 1, &wc);
		else 
			ret = ibv_poll_cq(n->rcv_cq, 1, &wc);

		if (ret < 0) {
			debug("error by ibv_poll_cq (error %d)", ret);
			return 1;
		}
	}while (ret == 0);

	if (wc.status != IBV_WC_SUCCESS) {
		debug("work completion status %s\n",
				ibv_wc_status_str(wc.status));
		return 1;
	}

	return 0;
}
