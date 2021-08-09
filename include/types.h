

#ifndef __MC_TYPES_H__
#define __MC_TYPES_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define MAX_NODE 10
#define BUFFER_SIZE (1ul << 30)
#define MAX_CTRL 10

#define SEND 0
#define RECV 1

#define bool short
#define true 1
#define false 0

#define TEST_NZ(x) do {if ((x)) die("error :" #x "failed(return non-zero)." ); }while(0)
#define TEST_Z(x) do {if (!(x)) die("error :" #x "failed(return zero/null)."); }while(0)
#define dump_stat() printf("%d thread:  %s\n",gctrl->nr_node ,__func__)

#define rdma_error(msg, args...) do {\
	fprintf(stderr, "%s : %d : ERROR : "msg, __FILE__, __LINE__, ## args);\
}while(0);

#define debug(msg, args...) do {\
    printf("DEBUG: "msg, ## args);\
}while(0);
enum node_type{
	SERVER,
	CLIENT,
};

struct device{
	struct ibv_pd *pd;
	struct ibv_context *verbs;
};

struct node{
	
	struct ctrl *ctrl;

	struct ibv_qp *qp;
	struct ibv_cq *rcv_cq;
	struct ibv_cq *snd_cq;
	struct ibv_comp_channel *rcv_cc;
	struct ibv_comp_channel *snd_cc;
	struct rdma_cm_id *id;
	struct ibv_ah * ah ;
	//socket addr; 
	bool mc_join;
	bool type;
	enum{
		INIT,
		CONNECTED,
		GENRETED
	}state;

	struct ibv_mr * mr;
	void * buffer;

	uint64_t remote_qpn;
	uint32_t remote_qkey;

};

struct ctrl{
	
	struct rdma_event_channel *ec ;
	struct rdma_cm_id *id;
	struct node * node;
	struct device * dev;
	int nr_node ;

	struct sockaddr mcast_sockaddr;

	bool type;
	char *bind_addr;
    char *mcast_addr;
    char *server_port;
	//struct rdma_addrinfo *dest_addr;
    //struct rdma_addrinfo *mcast_addr;

};

struct memregion{
	
	uint64_t baseaddr;
	uint32_t key;
};

#endif
