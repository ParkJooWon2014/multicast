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
	rdma_error("%s - errno: %d\n", reason, errno);
	exit(EXIT_FAILURE);
}

static struct ctrl *gctrl = NULL;

struct ctrl *get_ctrl(void)
{
	return gctrl;
}

struct device * get_device(struct node *node)
{
	struct device *dev = NULL;
//	struct ibv_device **ibv_dev = NULL;
//	int num = 0 ;
//	struct ibv_context * verbs = NULL;	
	
	if(!node->ctrl->dev){
		
		dev = (struct device *) malloc(sizeof(*dev));
		TEST_Z(dev);
		
		if(node->type == CLIENT){
//			TEST_Z(ibv_dev = ibv_get_device_list(&num));
//			TEST_Z(verbs = ibv_open_device(ibv_dev[0]));
//			node->cm_id->verbs = verbs;
		}

		dev->verbs = node->cm_id->verbs;
		TEST_Z(dev->verbs);
		dev->pd = ibv_alloc_pd(dev->verbs);
		TEST_Z(dev->pd);

		node->ctrl->dev = dev;
	}

	return node->ctrl->dev;
}
static void create_qp(struct node * node)
{
	struct ibv_qp_init_attr qp_attr = {};

	qp_attr.send_cq = node->snd_cq;
	qp_attr.recv_cq = node->rcv_cq;
	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.cap.max_send_wr = 128;
	qp_attr.cap.max_recv_wr = 128;
	qp_attr.cap.max_send_sge = 16;
	qp_attr.cap.max_recv_sge = 16;

	TEST_NZ(rdma_create_qp(node->cm_id, node->ctrl->dev->pd, &qp_attr));
	node->qp = node->cm_id->qp;
}

static void create_cq(struct node *node)
{
	struct ibv_context * ctx = node->ctrl->dev->verbs;

	TEST_Z(node->rcv_cc = ibv_create_comp_channel(ctx));
	TEST_Z(node->snd_cc = ibv_create_comp_channel(ctx));
	TEST_Z(node->rcv_cq = ibv_create_cq(ctx,1,NULL,node->rcv_cc,0));
	TEST_Z(node->snd_cq = ibv_create_cq(ctx,1,NULL,node->snd_cc,0));

}

void init_node(struct node * node)
{
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
	struct node * node = (struct node *)malloc(sizeof(*node));

	TEST_Z(node);
	memset(node, 0,sizeof(*node));

	node->type = type;
	node->state = INIT;
	node->ctrl = ctrl;

	return node;
}

struct ctrl* alloc_control()
{
	
	struct ctrl *ctrl = (struct ctrl*)malloc(sizeof(*ctrl));
	TEST_Z(ctrl);
	memset(ctrl,0,sizeof(*ctrl));
	ctrl->nr_node = 0;
	ctrl->dev = NULL;

	return ctrl;
}


static int on_disconnect(struct node * node)
{
	//struct ctrl *ctrl = node->ctrl;

//	if(node->state == CONNECTED){

		node->state = INIT;

	rdma_destroy_qp(node->cm_id);
	rdma_destroy_id(node->cm_id);

//	}
	return 1;
}

static int on_multicast(struct node * node)
{
	
//	struct ctrl *ctrl = node->ctrl;
	node->mc_join = true;

	return 0;
}

static int on_connect_request(struct rdma_cm_id *id, 
		struct rdma_conn_param *param)
{

	struct rdma_conn_param cm_params = {};
	struct ibv_device_attr attrs = {};
	
	if(!gctrl)
		gctrl = alloc_control();

	struct node *node =  alloc_node(gctrl,SERVER); // gctrl->node[gctrl->nr_node++];
	init_node(node);

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

static int on_connection(struct node *node)
{
	post_recv(node);
	return 1 ;
}

void destroy_device(struct ctrl *ctrl)
{
	TEST_Z(ctrl->dev);
	/*modify*/
	ibv_dereg_mr(ctrl->node[0]->mr);
	free(ctrl->node[0]->buffer);
	/*modify*/
	ibv_dealloc_pd(ctrl->dev->pd);
	free(ctrl->dev);
	ctrl->dev = NULL;
}
int process_rdma_cm_event(struct rdma_event_channel *echannel,
		enum rdma_cm_event_type expected_event,
		struct rdma_cm_event **cm_event)
{
	int ret = 1;
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
int rdam_multi_solve(struct rdma_cm_id * id , struct rdma_event_channel *ec, 
		struct sockaddr_in *server)
{

	int ret ;
	struct rdma_cm_event *cm_event = NULL;
	
	TEST_NZ(rdma_resolve_addr(id,NULL,(struct sockaddr*)server,2000));
	
	ret = process_rdma_cm_event(ec,RDMA_CM_EVENT_ADDR_RESOLVED,&cm_event);
	
	TEST_NZ(rdma_resolve_route(id, 2000));
	
	debug("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
	
	ret = process_rdma_cm_event(ec, 
			RDMA_CM_EVENT_ROUTE_RESOLVED,
			&cm_event);
	
	if (ret) {
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	
	printf("Trying to connect to server at : %s port: %d \n", 
			inet_ntoa(server->sin_addr),
			ntohs(server->sin_port));	
	
	return ret ;
}
struct ctrl*  rdma_client_create(struct rdma_cm_id *id, struct sockaddr *src_addr,
		struct sockaddr *dst_addr)
{
	struct ctrl* ctrl = alloc_control();
	struct node* node = alloc_node(ctrl,CLIENT);
	//struct rdma_cm_event *cm_event;

	node->cm_id = id;
	id->context = node;
	

	struct device *dev = get_device(node);
	init_node(node);
	struct ibv_device_attr attrs = {};
	
	if(ctrl->nr_node >=MAX_NODE)
		return NULL;
	
	ctrl->node[ctrl->nr_node] = node;
	ctrl->nr_node +=1 ;

	create_cq(node);
	create_qp(node);
	
	TEST_NZ(ibv_query_device(dev->verbs, &attrs));
	
	debug("attrs: max_qp=%d, max_qp_wr=%d, max_cq=%d max_cqe=%d \
			max_qp_rd_atom=%d, max_qp_init_rd_atom=%d\n", attrs.max_qp,
			attrs.max_qp_wr, attrs.max_cq, attrs.max_cqe,
			attrs.max_qp_rd_atom, attrs.max_qp_init_rd_atom);


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

	return ctrl;
}

int on_event(struct rdma_cm_event *event)
{
	struct node * node = (struct node*)event->id->context;

	switch(event->event)
	{
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			return on_connect_request(event->id,&event->param.conn);
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
