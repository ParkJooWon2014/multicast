#include "types.h"
#include "ib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
void die(const char *reason)
{
	fprintf(stderr,"%s - errno: %d\n", reason, errno);
	exit(EXIT_FAILURE);
}

static struct ctrl *gctrl = NULL;

struct ctrl *get_ctrl(void)
{
	return gctrl;
}

struct device * get_device(struct node *node)
{
	debug("%s START \n ",__func__);
	struct device *dev = NULL;
	
	if(!node->ctrl->dev){
		
		dev = (struct device *) malloc(sizeof(*dev));
		TEST_Z(dev);
		
		dev->verbs = node->cm_id->verbs;
		TEST_Z(dev->verbs);
		dev->pd = ibv_alloc_pd(dev->verbs);
		TEST_Z(dev->pd);

		node->ctrl->dev = dev;
	}

	return node->ctrl->dev;
}

int init_qp(struct node *node)
{

	struct ibv_context *ctx = node->ctrl->dev->verbs;
	struct ibv_cq *rcv_cq = NULL;
	struct ibv_cq *snd_cq = NULL ;
	struct ibv_qp *qp = NULL;
	struct ibv_comp_channel * snd_cc = NULL;
	struct ibv_comp_channel * rcv_cc = NULL;

	int flags;
	int psn = lrand48() & 0xffffff;
	{
		snd_cc =ibv_create_comp_channel(ctx);
		rcv_cc =ibv_create_comp_channel(ctx);
		rcv_cq = ibv_create_cq(ctx, 1, node, rcv_cc, 0);
		snd_cq = ibv_create_cq(ctx, 1, node, snd_cc, 0);
		if (!rcv_cq || !snd_cq) {
			debug("Cannot create cq\n");
			return 1;
		}
		node->rcv_cc = rcv_cc;
		node->snd_cc =  snd_cc;
		node->rcv_cq = rcv_cq;
		node->snd_cq = snd_cq;

	}

	
		struct ibv_qp_init_attr qattr = {
			.qp_type = IBV_QPT_UD,
			.send_cq = snd_cq,
			.recv_cq = rcv_cq,
			.cap = {
				.max_send_wr = 128,
				.max_recv_wr = 128, /* rx_depth */
				.max_send_sge = 16,
				.max_recv_sge = 16,
			},
			.srq = NULL ,
		};
		qp = ibv_create_qp(node->ctrl->dev->pd, &qattr);
		if (!qp) {
			rdma_error("Unable to create qp");
		}
		node->qp = qp;
		debug("qpn: %d, psn: %d\n", qp->qp_num, psn);
	
	
		/* Retrieve the IDs */
		struct ibv_port_attr pattr;
		int ret;
		bzero(&pattr, sizeof(pattr));
		ret = ibv_query_port(
				ctx, 1 /* 1-base index for port number */, &pattr);
		if (ret) {
			ibv_close_device(ctx);
			rdma_error("Cannot query port attributes\n");
		}
	
	
		struct ibv_qp_attr attr = {
			.qp_state = IBV_QPS_INIT,
			.pkey_index = 0,
			.port_num = 1,
			.qp_access_flags =  IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE,
			.qkey = 0x11111111,

		};
		flags = IBV_QP_STATE;
		flags |= IBV_QP_QKEY | IBV_QP_PKEY_INDEX | IBV_QP_PORT ;

		if (ibv_modify_qp(qp, &attr, flags)) {
			rdma_error("Unable to set INIT\n");
			return 1 ;
		}

	
		struct ibv_qp_attr rattr = {
			.qp_state = IBV_QPS_RTR,
			.path_mtu = IBV_MTU_4096,
			.ah_attr = {
				.src_path_bits = 0,
				.port_num = 1,
				.dlid = 0,		/* destination lid */
				.is_global = 0,
			},
			.dest_qp_num = 0,	/* dest_qpn */
			.rq_psn = 0,		/*dest->psn */
			.max_dest_rd_atomic = 1,
			.min_rnr_timer = 12,
		};

		flags = IBV_QP_STATE;
//		flags |= IBV_QP_PATH_MTU;
//		flags |= IBV_QP_AV | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN;
//		flags |= IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

		if (ibv_modify_qp(qp, &rattr, flags )) {
			rdma_error("Unable to set RTR\n");
			return 1;
		}

		
		struct ibv_qp_attr cattr = {
			.qp_state = IBV_QPS_RTS,
			.sq_psn = psn, /* my->psn */
			.timeout = 14,
			.retry_cnt = 7,
			.rnr_retry = 7,
			.max_rd_atomic = 1,
		};
		flags = IBV_QP_STATE;
		flags |= IBV_QP_SQ_PSN;
//		flags |= IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;

		if (ibv_modify_qp(qp, &cattr, flags)) {
			rdma_error("Unable to set RTS\n");
			return  1;
		}
		struct ibv_ah_attr Aattr;
		bzero(&Aattr,sizeof(Aattr));
		Aattr.is_global = 0;
		Aattr.dlid = 0;
		Aattr.sl  = 0 ;
		Aattr.port_num = 1 ;
		struct ibv_ah *ah = ibv_create_ah(node->ctrl->dev->pd,&Aattr);
		if(!ah){
			rdma_error("Unable to create AH\n");
			return 1 ;
		}
		node->ah = ah ;
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

	TEST_NZ(rdma_create_qp(node->cm_id, node->ctrl->dev->pd, &qp_attr));
	node->qp = node->cm_id->qp;

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
void get_node_mr(struct node * node)
{
	debug("%s START \n ",__func__);
	
	TEST_Z(node);
	struct device * dev = node->ctrl->dev;

	node->buffer = malloc(BUFFER_SIZE);
	TEST_Z(node->buffer);

	TEST_Z(node->mr = ibv_reg_mr(
				dev->pd, node->buffer,BUFFER_SIZE,
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)
			);
	debug("registered memory region of %zu bytes\n", BUFFER_SIZE);

}
struct node *alloc_node(struct ctrl *ctrl,bool type)
{

	debug("%s START \n ",__func__);
	
	struct node * node = (struct node *)malloc(sizeof(*node));

	TEST_Z(node);
	memset(node, 0,sizeof(*node));

	node->type = type;
	node->state = INIT;
	node->ctrl = ctrl;

	return node;
}

struct ctrl* alloc_ctrl(bool type)
{
	debug("%s START \n",__func__);

	struct ctrl * ctrl  = NULL ;//  = (struct ctrl * ctrl)malloc(sizeof(*ctrl));
	//struct node * node = NULL;

	ctrl  = (struct ctrl *)malloc(sizeof(*ctrl));
	TEST_Z(ctrl);
	memset(ctrl,0,sizeof(*ctrl));
	ctrl->nr_node = 0 ;
	ctrl->dev =NULL;
	ctrl->bind = type;

	ctrl->node = alloc_node(ctrl,type);


	return ctrl; 
}
int get_addrinfo(struct ctrl*ctrl , struct sock_addr *sock,bool type)
{

	if(type == CLIENT){
		ctrl->server.sin_family = AF_INET;
		ctrl->server.sin_addr.s_addr = inet_addr(sock->dest_addr);
		ctrl->server.sin_port = htons(atoi(sock->server_port));
	}
	ctrl->client.sin_family =AF_INET;
	ctrl->client.sin_port = htons(atoi("50000"));

	ctrl->mcast_sockaddr.sin_family = AF_INET;
	ctrl->mcast_sockaddr.sin_addr.s_addr = inet_addr(sock->mcast_addr);
	ctrl->mcast_sockaddr.sin_port = htons(atoi("50000"));

	return 0;

}
static int on_disconnect(struct node * node)
{

	debug("%s START \n ",__func__);
	
	node->state = INIT;

	rdma_destroy_qp(node->cm_id);
	rdma_destroy_id(node->cm_id);

	return 1;
}

static int on_multicast(struct node * node)
{
	
	debug("%s START \n ",__func__);
//	struct ctrl *ctrl = node->ctrl;
	node->mc_join = true;

	return 0;
}
/*
static int on_connect_request(struct rdma_cm_id *id, 
		struct rdma_conn_param *param)
{

	debug("%s START \n ",__func__);
	
	struct rdma_conn_param cm_params = {};
	struct ibv_device_attr attrs = {};
	
	if(!gctrl)
		gctrl = alloc_control();

	struct node *node =  alloc_node(gctrl,SERVER); // gctrl->node[gctrl->nr_node++];
	get_node_mr(node);

	TEST_Z(node->state == INIT);

	id->context = node;
	node->cm_id = id;

	struct device *dev = get_device(node);

	create_cq(node);
	create_qp(node);

	if(gctrl->nr_node >= MAX_NODE)
		return 1;

	gctrl->node[gctrl->nr_node] = node;
	gctrl->nr_node += 1;

	TEST_NZ(ibv_query_device(dev->verbs, &attrs));
	
	debug("attrs: max_qp=%d, max_qp_wr=%d, max_cq=%d max_cqe=%d \
			max_qp_rd_atom=%d, max_qp_init_rd_atom=%d\n", attrs.max_qp,
			attrs.max_qp_wr, attrs.max_cq, attrs.max_cqe,
			attrs.max_qp_rd_atom, attrs.max_qp_init_rd_atom);

	debug("ctrl attrs: initiator_depth=%d responder_resources=%d\n",
			param->initiator_depth, param->responder_resources);

	cm_params.initiator_depth = param->initiator_depth;
	cm_params.responder_resources = param->responder_resources;
	cm_params.rnr_retry_count = param->rnr_retry_count;
	cm_params.flow_control = param->flow_control;

	TEST_NZ(rdma_accept(node->cm_id, &cm_params));

	return GENRETED;
}
*/
static int on_connection(struct node *node)
{

	debug("%s START \n ",__func__);
	
	mpost_recv(node);
	return 0 ;
}

void destroy_device(struct ctrl *ctrl)
{

	debug("%s START \n ",__func__);
	
	TEST_Z(ctrl->dev);
	/*modify*/
	ibv_dereg_mr(ctrl->node->mr);
	free(ctrl->node->buffer);
	/*modify*/
	ibv_dealloc_pd(ctrl->dev->pd);
	free(ctrl->dev);
	ctrl->dev = NULL;
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
int rdma_multi_solve(struct rdma_cm_id * id , struct ctrl* ctrl)
{

	debug("%s START \n ",__func__);
	
	int ret ;
	struct rdma_cm_event *cm_event = NULL;	
	struct rdma_event_channel *ec = ctrl->ec;

	if(ctrl->bind == SERVER){
		TEST_Z(rdma_bind_addr(id,(struct sockaddr*)&ctrl->client));
	}

	TEST_NZ(rdma_resolve_addr(id,/*(struct sockaddr*)&ctrl->client*/ NULL,(struct sockaddr*)&ctrl->mcast_sockaddr,2000));	
	ret = process_rdma_cm_event(ec,RDMA_CM_EVENT_ADDR_RESOLVED,&cm_event);

	TEST_NZ(rdma_resolve_route(id, 2000));
	
//	debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
	
	ret = process_rdma_cm_event(ec, 
			RDMA_CM_EVENT_ROUTE_RESOLVED,
			&cm_event);
	
	if (ret) {
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	/*
	debug("Trying to connect to server at : %s port: %d \n", 
			inet_ntoa(ctrl->server.sin_addr),
			ntohs(ctrl->server.sin_port));	

*/
	return ret ;
}

struct node * rdma_create_node(struct rdma_cm_id * id , struct ctrl *ctrl)
{
	
	TEST_Z(ctrl);
	int ret = 1 ;
	struct node * node = NULL;
	struct ibv_device_attr attrs ={};
	struct rdma_cm_event * event;
	
	node = ctrl->node ;
	node->cm_id = id ; 
	id->context = node ;
	
	get_device(node);
	get_node_mr(node);

	create_cq(node);
	create_qp(node);

	TEST_NZ(ibv_query_device(ctrl->dev->verbs, &attrs));

    debug("attrs: max_qp=%d, max_qp_wr=%d, max_cq=%d max_cqe=%d \
            max_qp_rd_atom=%d, max_qp_init_rd_atom=%d\n", attrs.max_qp,
            attrs.max_qp_wr, attrs.max_cq, attrs.max_cqe,
            attrs.max_qp_rd_atom, attrs.max_qp_init_rd_atom);

	if(ctrl->bind){

		struct rdma_conn_param param = {
			.qp_num = node->qp->qp_num,
			.flow_control = 1,
			.responder_resources = 16,
			.initiator_depth = 16,
			.retry_count = 7,
			.rnr_retry_count = 7,
		    .private_data = NULL,
			.private_data_len = 0,
	    };

		 TEST_NZ(rdma_connect(id,&param));
	};
	
	ret = process_rdma_cm_event(ctrl->ec,RDMA_CM_EVENT_ADDR_RESOLVED,&event);
	
	ret = rdma_join_multicast(id,(struct sockaddr*)&ctrl->mcast_sockaddr, NULL);
	if(ret)
	{
		rdma_error("rdma_join_multicast  is failed\n");
		return NULL ;
	}
	ret = process_rdma_cm_event(ctrl->ec,RDMA_CM_EVENT_MULTICAST_JOIN,&event);
	if(ret){
		rdma_error("process_rdma_cm_event got faults\n");
		return NULL;
	}
	node->mc_join = true;
	node->remote_qpn =event->param.ud.qp_num;
	node->remote_qkey = event->param.ud.qkey;

	TEST_Z(node->ah = ibv_create_ah(ctrl->dev->pd,&event->param.ud.ah_attr));

	return node; 
}

int on_event(struct rdma_cm_event *event)
{
	debug("%s START \n ",__func__);
	
	struct node * node = (struct node*)event->id->context;

	switch(event->event)
	{
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			return 0;//on_connect_request(event->id,&event->param.conn);
		case RDMA_CM_EVENT_ESTABLISHED:
			return on_connection(node);
		case RDMA_CM_EVENT_MULTICAST_JOIN:
			return on_multicast(node);
		case RDMA_CM_EVENT_DISCONNECTED:
			on_disconnect(node);
			return 1;
		default :
			debug("unknown event : %s\n",rdma_event_str(event->event));
			return 1;
	}

}
