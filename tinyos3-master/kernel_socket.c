
#include "tinyos.h"
#include "kernel_socket.h"

file_ops socket_file_ops = {
	.Open = (void *) socket_open,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

Socket_CB* Port_Map[MAX_PORT] = {NULL};


Fid_t sys_Socket(port_t port)
{
	return NOFILE;
}

int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	/*Checks to see if data entered on function call is legal*/
	if(sock < 0 || sock > MAX_FILEID)
	{
		return -1;
	}

	if(port < 0 || port > MAX_PORT)
	{
		return -1;
	}

	if(Port_Map[port] == NULL || Port_Map[port]->type != SOCKET_LISTENER)
	{
		return -1;
	}

	/*Acquire the fcb from the fid inserted on function call*/
	FCB* fcb = get_fcb(sock);
	if(fcb == NULL)
	{
		return -1;
	}

	/*Acquire the socket from the streamobj of the fcb*/
	Socket_CB* socket_cb = (Socket_CB *)fcb->streamobj;
	
	/*Check if socket is legal*/
	if(socket_cb == NULL || socket_cb->type != SOCKET_UNBOUND)
	{
		return -1;
	}

	/*Acquire the listen socket based on the port given on function call*/
	Socket_CB* listen_socket = Port_Map[port];
	/*New request to be handled*/
	socket_cb->refcount++;

	/*Build the connection request*/
	connection_request* req = (connection_request *)xmalloc(sizeof(connection_request));
	req->admitted = 0;
	req->peer = socket_cb;
	req->connected_cv = COND_INIT;
	rlnode_init(&req->queue_node, socket_cb);

	/*Make the node of the finished request to be pushed*/
	rlnode request_node;
	rlnode_init(&request_node, req);

	/*Add the request node to the list of the listener socket*/
	rlist_push_front(&listen_socket->listener_s.queue, &request_node);
	/*Awake up the listening socket who might be waiting on an available request*/
	kernel_signal(&listen_socket->listener_s.req_available);

	/*Wait until timeout for a response*/
	if(kernel_timedwait(&listen_socket->listener_s.req_available,SCHED_IO, timeout))
	{
		/*if request is still 0 and not 1, the request was not accepted*/
		if(req->admitted == 0)
		{
			/*Request not accepted*/
			socket_cb->refcount--;
			return -1;
		}
		/*Request handled*/
		socket_cb->refcount--;
		return 0;
	}else{
		/*Request timedout*/
		socket_cb->refcount--;
		return -1;
	}

	socket_cb->refcount--;
	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	/*Checks to see if data entered on function call is legal*/
	if(sock < 0 || sock > MAX_FILEID)
	{
		return -1;
	}

	/*Acquire the fcb from the fid inserted on function call*/
	FCB* fcb = get_fcb(sock);
	/*Check if fcb is legal*/
	if(fcb == NULL)
	{
		return -1;
	}

	/*Acquire the socket from the streamobj of the fcb*/
	Socket_CB* socket_cb = (Socket_CB *)fcb->streamobj;
	/*Check if socket is legal*/
	if(socket_cb == NULL)
	{
		return -1;
	}

	/*Based on value of how from function call, handle shutdowns appropriately*/
	if(how == SHUTDOWN_READ)
	{
		return pipe_reader_close(socket_cb->peer_s.read_pipe);
	}
	if(how == SHUTDOWN_WRITE)
	{
		return pipe_writer_close(socket_cb->peer_s.write_pipe);
	}
	if(how == SHUTDOWN_BOTH)
	{
		int res = 0;
		res = pipe_reader_close(socket_cb->peer_s.read_pipe) + pipe_writer_close(socket_cb->peer_s.write_pipe);
		/*if res is -1 or -2 one or both pipe end closures failed*/
		if(res == 0)
		{
			return 0;
		}else{
			return -1;
		}
	}
	return 0;
}

int socket_open()
{
	return -1;
}

int socket_read()
{
	return -1;
}

int socket_write()
{
	return -1;
}

int socket_close()
{
	return -1;
}
