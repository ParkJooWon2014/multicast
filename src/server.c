#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <multicast.h>
#include <ib.h>
int main(int argc, char **argv)
{

	uint16_t  port = 0;
	int ret = 0; 
	//	int opt ;
	struct sockaddr_in addr = {};
	struct rdma_cm_event *event = NULL;
	struct rdma_event_channel *ec = NULL;
	struct rdma_cm_id *listener = NULL;
	struct ctrl * ctrl;

	if (argc != 2) {
		die("Need to specify a port number to listen");
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));

	TEST_Z(ec = rdma_create_event_channel());
	TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
	TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
	TEST_NZ(rdma_listen(listener,MAX_NODE));
	port = ntohs(rdma_get_src_port(listener));
	printf("listening on port %d.\n", port);


	// handle connection requests
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

	// handle disconnects, etc.
	rdma_destroy_event_channel(ec);
	rdma_destroy_id(listener);
	destroy_device(ctrl);
	return 0;
}
/*
//client_mode 
int main()
{
	
	struct sockaddr_in addr = {};
	struct rdma_cm_event *event = NULL;
	struct rdma_event_channel *ec = NULL;
	struct rdma_cm_id *id = NULL;
	struct ctrl * ctrl;
	struct rdma_conn_param conn_param;

	if (argc != 2) {
		die("Need to specify a port number to listen");
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));

	TEST_Z(ec = rdma_create_event_channel());
	TEST_NZ(rdma_create_id(ec, &id, NULL, RDMA_PS_TCP));
	port = ntohs(rdma_get_src_port(listener));
	TEST_Z(ctrl = rdam_create_);
	printf("READY  on port %d.\n", port);


	return 0;
}
*/
