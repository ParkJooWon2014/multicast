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
#include <pthread.h>
#define msg_size 1024
#define DEFAULT_PORT "50000"

int main(int argc, char *argv[])
{
	int  op;
	int ret = 0 ;
	struct ctrl * ctrl = NULL;

	
	TEST_Z(ctrl = alloc_ctrl());
    ctrl->server_port = DEFAULT_PORT;
	ctrl->type =SERVER;
    
	while ((op = getopt(argc, argv, "chb:m:p:")) != -1)
    {
        switch (op)
        {
        case 'c':
            ctrl->type = CLIENT;
            break;
        case 'b':
            ctrl->bind_addr = optarg;
            break;
        case 'm':
            ctrl->mcast_addr = optarg;
            break;
        case 'p':
            ctrl->server_port = optarg;
            break;
        default:
            printf("usage: %s -m mc_address\n", argv[0]);
            printf("\t[-c client mode]\n");
            printf("\t[-b bind_address]\n");
            printf("\t[-p port_number]\n");
            exit(1);
        }
    }

    if (ctrl->mcast_addr == NULL)
    {
        printf("multicast address must be specified with -m\n");
        exit(1);
    }
	ret = resolve_addr(ctrl);
	if(ret){
		debug("Can't resolve addr \n");
		exit(1);
	}
	ret = rdma_create_node(ctrl);
	if(ret){
		debug("rdma_creat is failed\n");
		exit(1);
	}

	if(ctrl->type){
		char buffer[100]  = "THIS IS SSLAB COMPUTER \n";
		int count = 0 ;
	//	int buffer =  12348123;
		while(count < 100 ){
			printf("+++++COUNT :%d+++++\n",count);
			debug("sending test MSG..... \n");
			post_send(ctrl->node,buffer,100);
			debug("MSG : %s\n",buffer);
			printf("\n");
			count ++;
		}
	}

	else {
		while(1)
			sleep(1);
	}

	destory_ctrl(ctrl);
	debug("CLIENT IS OVER\n");
	return 0;

}

