#include "types.h"
#include "ib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
int debug_count = 0 ;
#define DEBUG() debug("STAGE %d : %s  %d \n",debug_count++,__func__,__LINE__)

void die(const char *reason)
{
	fprintf(stderr,"%s - errno: %d\n", reason, errno);
	exit(EXIT_FAILURE);
}

static void *cm_thread(void *arg)
{
    struct rdma_cm_event *event;
    int ret;
    struct ctrl *ctrl = (struct ctrl *)arg;
    while (1)
    {
        ret = rdma_get_cm_event(ctrl->ec, &event);
        if (ret)
        {
            debug("rdma_get_cm_event\n");
            break;
        }
        printf("event %s, status %d\n",
               rdma_event_str(event->event), event->status);
        rdma_ack_cm_event(event);
    }
    return NULL;
}
static int recv_event(struct ctrl *ctrl)
{
	pthread_t event ; 
	pthread_attr_t attr;
	pthread_attr_init(&attr); 
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);	
	return pthread_create(&event,&attr,cm_thread,(void*)ctrl);
}
int process_rdma_cm_event(struct rdma_event_channel *echannel,
		enum rdma_cm_event_type expected_event,
		struct rdma_cm_event **cm_event)
{

	debug("%s START \n ",__func__);
	
	int ret = 1;

	debug("ready to receive %s type event \n", rdma_event_str(expected_event));// rdma_event_str((*cm_event)->event));
	ret = rdma_get_cm_event(echannel, cm_event);
	
	if (ret) {
		rdma_error("Failed to retrieve a cm event, errno: %d \n",
				-errno);
		return -errno;
	}
	
	if(0 != (*cm_event)->status){
		rdma_error("CM event has non zero status: %d\n", (*cm_event)->status);
		ret = -((*cm_event)->status);
		rdma_ack_cm_event(*cm_event);
		return ret;
	}
	
	if ((*cm_event)->event != expected_event) {
		rdma_error("Unexpected event received: %s [ expecting: %s ]",
				rdma_event_str((*cm_event)->event),
				rdma_event_str(expected_event));
		rdma_ack_cm_event(*cm_event);
		return -1;
	}
	
	debug("A new %s type event is received \n", rdma_event_str((*cm_event)->event));

	
	ret = rdma_ack_cm_event(*cm_event);	
	
	if (ret) {
		rdma_error("Failed to acknowledge the CM event, errno: %d\n", -errno);
		return -errno;
	}
	
	return ret;
}

int resolve_addr(struct ctrl *ctrl)
{
    int ret;
	struct rdma_cm_id *id= ctrl->id;
	struct rdma_event_channel*ec  = ctrl->ec ;
    struct rdma_addrinfo *bind_rai = NULL;
    struct rdma_addrinfo *mcast_rai = NULL;
    struct rdma_addrinfo hints;
	struct rdma_cm_event *event = NULL;
	struct sockaddr_in server_addr;

	memset(&hints, 0, sizeof(hints));
    hints.ai_port_space = RDMA_PS_UDP;
    if (ctrl->bind_addr)
    {
        hints.ai_flags = RAI_PASSIVE;
        ret = rdma_getaddrinfo(ctrl->bind_addr, NULL, &hints, &bind_rai);
        if (ret)
        {
			rdma_error("rdma_getaddrinfo (bind)\n");
            return ret;
        }
    }
    hints.ai_flags = 0;
    ret = rdma_getaddrinfo(ctrl->mcast_addr, NULL, &hints, &mcast_rai);
    if (ret)
    {
        rdma_error("rdma_getaddrinfo (mcast)\n");
        return ret;
    }
	if(ctrl->server_addr){
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(atoi(ctrl->server_port));
		server_addr.sin_addr.s_addr = inet_addr(ctrl->server_addr);
	}

    if (ctrl->bind_addr)
    {
        ret = rdma_bind_addr(id, bind_rai->ai_src_addr);
        
		if (ret)
        {
            rdma_error("rdma_bind_addr\n");
            return ret;
        }
		debug("rdma bind addr is Success\n");
    }
	else{
		struct sockaddr_in mine;
		bzero(&mine,sizeof(mine));
		mine.sin_family = AF_INET;
		mine.sin_port = htons(50000);
		
        ret = rdma_bind_addr(id, (struct sockaddr*)&mine);
		if (ret)
        {
            rdma_error("rdma_bind_addr\n");
            return ret;
		}
		debug("rdma bind addr is Success\n");
	}
	if(ctrl->type == CLIENT){
		ret = rdma_resolve_addr(id, (bind_rai) ? bind_rai->ai_src_addr : NULL,
				                (struct sockaddr*)&server_addr,
								//mcast_rai->ai_dst_addr, 
								2000);
	
		if (ret)
		{
			rdma_error("rdma_resolve_addr\n");
			return ret;
		}
		ret = process_rdma_cm_event(ec, RDMA_CM_EVENT_ADDR_RESOLVED, &event);
		if (ret)
		{
			return ret;
	    }

		TEST_NZ(rdma_resolve_route(id, 2000));
	
		debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
	
		ret = process_rdma_cm_event(ec, 
				RDMA_CM_EVENT_ROUTE_RESOLVED,
				&event);
	
		if (ret) {
			rdma_error("Failed to receive a valid event, ret = %d \n", ret);
			return ret;
		}
	}
	else {
		struct rdma_cm_event *event = NULL;
        
        ret = rdma_get_cm_event(ctrl->ec, &event);
		
		TEST_NZ(rdma_accept(ctrl->id, &event->param.conn));
        
		if (ret)
        {
            debug("rdma_get_cm_event\n");
            return 1 ;
		}
		printf("event %s, status %d\n",
               rdma_event_str(event->event), event->status);
        
		rdma_ack_cm_event(event);

	}
	memcpy(&ctrl->mcast_sockaddr,
           mcast_rai->ai_dst_addr,
           sizeof(struct sockaddr));
    
	return 0;
}

static void create_qp(struct node * node)
{
	
	debug("%s START \n ",__func__);
	
	struct ibv_qp_init_attr qp_attr = {};
	bzero(&qp_attr,sizeof(qp_attr));
	qp_attr.send_cq = node->snd_cq;
	qp_attr.recv_cq = node->rcv_cq;
	qp_attr.qp_type = IBV_QPT_UD; // MAIN PROBLEM
	qp_attr.qp_context = node;
	qp_attr.cap.max_send_wr = 128;
	qp_attr.cap.max_recv_wr = 128;
	qp_attr.cap.max_send_sge = 16;
	qp_attr.cap.max_recv_sge = 16;

	TEST_NZ(rdma_create_qp(node->id, node->ctrl->dev->pd, &qp_attr));
	node->qp = node->id->qp;

}

static void create_cq(struct node *node)
{
	debug("%s START \n ",__func__);
	
	struct ibv_context * ctx = node->ctrl->dev->verbs;

	TEST_Z(node->rcv_cc = ibv_create_comp_channel(ctx));
	TEST_Z(node->snd_cc = ibv_create_comp_channel(ctx));
	TEST_Z(node->rcv_cq = ibv_create_cq(ctx,1,node,node->rcv_cc,0));
	TEST_Z(node->snd_cq = ibv_create_cq(ctx,1,node,node->snd_cc,0));
	
}
static void get_node_mr(struct node * node)
{
	debug("%s START \n ",__func__);
	
	TEST_Z(node);
	struct device * dev = node->ctrl->dev;

	node->buffer = malloc(BUFFER_SIZE);
	TEST_Z(node->buffer);
	TEST_Z(node->mr = //  rdma_reg_msgs(node->id,node->buffer,BUFFER_SIZE)
			ibv_reg_mr(
				dev->pd, node->buffer,BUFFER_SIZE,
				IBV_ACCESS_LOCAL_WRITE) // | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)
			);
	debug("registered memory region of %zu bytes\n", BUFFER_SIZE);

}

struct ctrl * alloc_ctrl(void)
{
	debug("%s START\n",__func__);

	struct ctrl* ctrl = NULL;
	
	ctrl = (struct ctrl *)malloc(sizeof(*ctrl));
	TEST_Z(ctrl);


	TEST_Z(ctrl->ec = rdma_create_event_channel());
	
	TEST_NZ(rdma_create_id(ctrl->ec,&ctrl->id,NULL,RDMA_PS_UDP));
	
	ctrl->node = NULL;
	ctrl->dev = NULL;
	ctrl->bind_addr = NULL;
	ctrl->mcast_addr =NULL ;
	ctrl->server_port = NULL;
	ctrl->server_addr = NULL;
	ctrl->type = false;
	return ctrl;
}



static struct device  * alloc_device(struct ctrl *ctrl)
{

	debug("%s START ....\n",__func__);
	
	struct device * dev = ctrl->dev ;

	if(!dev){
		dev = (struct device*)malloc(sizeof(*dev));
		TEST_Z(dev);

		dev->verbs = ctrl->id->verbs;	
		TEST_Z(dev->verbs);
		dev->pd = ibv_alloc_pd(dev->verbs);
		TEST_Z(dev->pd);

	}

	return dev ;
}

struct node * alloc_node(struct ctrl * ctrl)
{

	debug("%s START\n",__func__);
	
	TEST_Z(ctrl);
	
	struct node *node = (struct node *)malloc(sizeof(*node));
	int ret = 1 ;
	struct rdma_cm_event *event = NULL;
	struct ibv_port_attr port_attr ;


	node->id = ctrl->id;
	node->mc_join = false;
	node->type = ctrl->type;
	node->ctrl = ctrl;

	ret = ibv_query_port(ctrl->dev->verbs,ctrl->id->port_num,&port_attr);
	if(ret){
		rdma_error("ibv_query_port is failed\n");
		return NULL;
	}

	create_cq(node);
	create_qp(node);
	get_node_mr(node);
	node->state = INIT;

	ret = rdma_join_multicast(ctrl->id,&ctrl->mcast_sockaddr,NULL);
	if(ret)
	{
		rdma_error("rdma_join_multicast failed\n");
		return NULL;
	}

	node->mc_join = true;

	ret = process_rdma_cm_event(ctrl->ec,RDMA_CM_EVENT_MULTICAST_JOIN,&event);
	if(ret){
		rdma_error("process_rdma_cm_event is failed\n");
		return NULL;
	}
	node->ah = ibv_create_ah(ctrl->dev->pd,&event->param.ud.ah_attr);	
	
    node->remote_qpn = event->param.ud.qp_num;
    node->remote_qkey = event->param.ud.qkey;
    debug("+++++++ CTRL : qpn %lx +++++++\n", node->remote_qpn);
    debug("+++++++ CTRL : qkey %x +++++++ \n", node->remote_qkey);
	if(!node->ah)
	{
		rdma_error("create_ah  is failed\n");
		return NULL;
	}

	struct rdma_conn_param param = {
			.qp_num = node->qp->qp_num,
			.flow_control = 1,
			.responder_resources = 16,
			.initiator_depth = 16,
			.retry_count = 3,
			.rnr_retry_count = 7,
			.private_data = NULL,
			.private_data_len = 0,
	};

	TEST_NZ(rdma_connect(node->id,&param));

	node->state = CONNECTED;
//	struct rdma_cm_event * no ;
//	rdma_ack_cm_event(no);
//	debug("AB new %s type event is received \n", rdma_event_str((event)->event));
	return node;
}


int rdma_create_node(struct ctrl * ctrl)
{

	ctrl->dev = alloc_device(ctrl);
	ctrl->node = alloc_node(ctrl);
	//if(post_recv(ctrl->node))
	//{
	//	debug("post_recv failed\n");
	//	return 1 ;
	//}
	if(recv_event((void*)ctrl))
	{
		debug("cm_thread failed\n");
		return 1 ;
	}
	return 0;
}

void destory_ctrl(struct ctrl * ctrl)
{
	ctrl->node->state = INIT;
	if(ctrl->node->ah)
		ibv_destroy_ah(ctrl->node->ah);
	if(ctrl->node->qp)
		rdma_destroy_qp(ctrl->id);
	if(ctrl->node->mr)
		ibv_dereg_mr(ctrl->node->mr);
	if(ctrl->node->rcv_cq)
		ibv_destroy_cq(ctrl->node->rcv_cq);
	if(ctrl->node->snd_cq)
		ibv_destroy_cq(ctrl->node->snd_cq);
	
	free(ctrl->node->buffer);
	free(ctrl->node);

	rdma_destroy_id(ctrl->id);
}
