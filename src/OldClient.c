#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <multicast.h>
#include <ib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>

#define msg_size 1024
int main(int argc, char *argv[])
{
	int  opt;
	struct rdma_event_channel *ec = NULL;
	struct rdma_cm_id *id = NULL;
	struct ctrl * ctrl = alloc_ctrl(CLIENT);
	struct sock_addr sock = {NULL};
	bool type = SERVER;
//	struct rdma_cm_event *cm_event = NULL;
//

//	if (argc != 3) {
//		debug("Need to specify a port number to listen");
//		return EXIT_FAILURE;
//	}
  while ((opt = getopt(argc, argv, "cd:m:p:c:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            type = CLIENT;
            break;
        case 'd':
            sock.dest_addr = optarg;
            break;
        case 'm':
            sock.mcast_addr = optarg;
            break;
        case 'p':
            sock.server_port = optarg;
			break;
        default:
            debug("usage: %s -m mc_address\n", argv[0]);
            debug("\t[-c client mode]\n");
            debug("\t[-d dest_address]\n");
            debug("\t[-p port_number]\n");
            debug("\t[-m mcast_addr]");
			exit(1);
        }
    }

	TEST_Z(ec = rdma_create_event_channel());
	TEST_NZ(rdma_create_id(ec, &id, NULL, RDMA_PS_UDP));
	ctrl->ec = ec ;
	get_addrinfo(ctrl,&sock,type);
	TEST_NZ(rdma_multi_solve(id , ctrl));
	TEST_Z(rdma_create_node(id,ctrl));	
//	debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");



	char buffer [] = "QUIT : THIS IS TEST FOR MULTICAST SSALB COMPUTER ";
	while(1){
		debug(".... sending test MSG \n");
		mpost_send(ctrl->node,buffer,strlen(buffer));
		debug("MSG : %s\n",buffer);
		
		if(!strncmp("QUIT",buffer,4))
			break;
	}
	rdma_destroy_event_channel(ec);
	rdma_destroy_id(id);
	destroy_device(ctrl);
	debug("CLIENT IS OVER\n");
	return 0;

}

