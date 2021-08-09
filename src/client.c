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
#define DEFAULT_PORT "50000"

int main(int argc, char *argv[])
{
	int  op;
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

	if(ctrl->type){
	
		char buffer [] = "QUIT : THIS IS TEST FOR MULTICAST SSALB COMPUTER ";
		while(1){
			debug(".... sending test MSG \n");
			mpost_send(ctrl->node,buffer,strlen(buffer));
			debug("MSG : %s\n",buffer);
		
			if(!strncmp("QUIT",buffer,4))
				break;
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

