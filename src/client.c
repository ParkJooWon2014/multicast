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


int main(int argc, char *argv[])
{
	//int port;
	int ret ;
	struct sockaddr_in server;
	//struct sockaddr_in client;
	struct rdma_event_channel *ec = NULL;
	struct rdma_cm_id *id = NULL;
	struct ctrl * ctrl;
	//struct rdma_cm_event *cm_event = NULL;
	struct rdma_cm_event *event = NULL;
	if (argc != 3) {
		die("Need to specify a port number to listen");
	}


	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_port = htons(atoi(argv[2]));

	printf("%s %d \n",argv[1],atoi(argv[2]));

	TEST_Z(ec = rdma_create_event_channel());
	TEST_NZ(rdma_create_id(ec, &id, NULL, RDMA_PS_TCP));
	 rdam_multi_solve(id , ec, &server);
	TEST_Z(ctrl = rdma_client_create(id,NULL,(struct sockaddr*)&server));	
	printf("Finished Connecting!\n");

	while (rdma_get_cm_event(ec, &event) == 0) {
		struct rdma_cm_event event_copy;

		memcpy(&event_copy, event, sizeof(*event));
		rdma_ack_cm_event(event);

		ret = on_event(&event_copy);

		if(ret == GENRETED)
			ctrl = get_ctrl();
		else if(ret)
			break;


	}
	
	rdma_destroy_event_channel(ec);
	rdma_destroy_id(id);
	destroy_device(ctrl);
	return 0;

}

