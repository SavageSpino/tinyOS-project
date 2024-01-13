
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


Fid_t fid

FCB* fcb;



int Fcbs_value = FCB_reserve(1, fid, fcb);



if(Fcbs_value == 1)
{

Socket_CB* s;
s=(Socket_CB*)xmalloc(sizeof(Socket_CB));
s->socket_type=u_socket;
s->refcount=0;
s->port=port;
s->fcb=fcb;
}
else 
{
return -1;	
}


return fid;}

int sys_Listen(Fid_t sock)
{
 FCB* curfile= CURPROC->FIDT[sock];

 void* adress_F=curfile->streamobj;
 Socket_CB* s;
 
 //make sure that the fid sock is  legal 
 if (sock<0 ||sock>MAX_PORT||sock==NOPORT)
 {
 return -1;	
 }

   //make sure that the address of the fcb of the fid sock is legal
 if (curfile==NULL)
  {
  return -1;		
  }
 
 //make sure that the adress of the socket is legal
 if (adress_F!=NULL)
 {
 s=adress_F;//IF THE CODE HAS ERROR THEN CHANGE THE LINE TO  s=(Socket_CB*)adress_F
 //check what is the type of socket 
  if (s.socket_type!=u_socket || s.port!=null)//check for port and if the socket is initialized
   {
    return -1;	
   }

  else
  {
  	s.socket_type=l_socket;
    s->listener_s.req_available=COND_INIT;
    rlnode_init(&s->l.listener_s.queue,NULL)  
    PORT_MAP[socket_cb->port]=socket_cb;
    return 0;
  }

}
else
{
return -1;	
}	
}

Fid_t sys_Accept(Fid_t lsock)
{FCB* curfile= CURPROC->FIDT[lsock];

 void* adress_F=curfile->streamobj;
 Socket_CB* s;
	 

   if (lsock<0 ||lsock>MAX_PORT||lsock==NOPORT)
 {
 return NOFILE; 
 }

//make sure that the adress of the socket is legal
 if (adress_F!=NULL)
 {
  s=adress_F;

 if (s.socket_type!=l_socket)//there is a chance this code snippet is wrong|try this->if(socketcb1->type!=SOCKET_LISTENER) 
 {
 return NOFILE; 
 }
 else
s->refcount++;

while(is_rlist_empty((&s->listener_s.queue)) && PORT_MAP[s->port]!=NULL )//wait for a request, wakes up when request is found
        kernel_wait(&s->listener_s.req_available,SCHED_PIPE); //SCHED_PIPE->Sleep at a pipe or socket

    if(PORT_MAP[s->port]==NULL){
        so->refcount--;  //new line
        return NOFILE;
    } //Check if the port is still valid (the socket may have been closed while we were sleeping)


    //Take the first connection request from the queue and try to honor it
    connection_request * first_req = rlist_pop_front(&s->listener_s.queue)->obj; //Take the first connection request from the queue

    first_req->admitted=1; //honor first connection request

    Fid_t listener_peer = sys_Socket(s->port);
    if(listener_peer==NOFILE){
        s->refcount--;//new line
        return NOFILE;
    }

    FCB *listener_peer_fcb = get_fcb(listener_peer);
    socket_cb *listener_peer_socket = listener_peer_fcb->streamobj;

    socket_cb *requested_peer_socket = firstReq->peer;

    FCB *requested_peer_fcb = firstReq->peer->fcb;

   
    //peer to peer connection, make connected sockets SOCKET_PEER(Connect the 2 peers)
    requested_peer_socket->type=SOCKET_PEER;
    requested_peer_socket->peer_s.peer=listener_peer_socket;
    listener_peer_socket->type=SOCKET_PEER;
    listener_peer_socket->peer_s.peer=requested_peer_socket;


    //create 2 pipe control blocks
    pipe_cb* pipecb1 = (pipe_cb*)xmalloc(sizeof(pipe_cb));
    pipe_cb* pipecb2 = (pipe_cb*)xmalloc(sizeof(pipe_cb));

    //(initialize the connection between 2 peers)
    pipecb1->reader = requested_peer_fcb; //frontistirio 9, sxima diafanias 8
    pipecb1->writer = listener_peer_fcb; //frontistirio 9, sxima diafanias 8

    pipecb1->has_space=COND_INIT;
    pipecb1->has_data=COND_INIT;
    pipecb1->r_position=0;
    pipecb1->w_position=0;
    pipecb1->usedSpace=0;

    //(initialize the connection between 2 peers)
    pipecb2->reader=listener_peer_fcb; //frontistirio 9, sxima diafanias 8
    pipecb2->writer=requested_peer_fcb; //frontistirio 9, sxima diafanias 8

    pipecb2->has_space=COND_INIT;
    pipecb2->has_data=COND_INIT;
    pipecb2->r_position=0;
    pipecb2->w_position=0;
    pipecb2->usedSpace=0;

    requested_peer_socket->peer_s.read_pipe=pipecb1;
    requested_peer_socket->peer_s.write_pipe=pipecb2;
    listener_peer_socket->peer_s.read_pipe=pipecb2;
    listener_peer_socket->peer_s.write_pipe=pipecb1;

    kernel_signal(&firstReq->connected_cv); //simiosis frontistirio 9, selida 6 kai 7
    socketcb1->refcount--; //decrease refcount
    return listener_peer; //return new socket file id on success

 }



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

socket_read((void* socket_cbt, const char *buf, uint n)){
Socket_CB* socket_cb = (Socket_CB*)socket_cbt;

if(socket_cb->type==l_socket||socket_cb->type==u_socket) 
{
return -1;	
}
else
{

Pipe_CB* pipeCB = (Pipe_CB*)socket_cb->p_socket.read_pipe;


return pipe_read(pipeCB,buf,n);

}

socket_write((void* socket_cbt, const char *buf, uint n)){
Socket_CB* socket_cb = (Socket_CB*)socket_cbt;

if(socket_cb->type==l_socket||socket_cb->type==u_socket) 
{
return -1;	
}
else
{

Pipe_CB* pipeCB = (Pipe_CB*)socket_cb->p_socket.write_pipe;


return pipe_write(pipeCB,buf,n);

}

//HELP !!!!
socket_close((void* socket_cbt, const char *buf, uint n)){
Socket_CB* socket_cb = (Socket_CB*)socket_cbt;
if(socket_cb==NULL)
{
    return -1;
} 
if(socket_cb->type==u_socket)
 {
    if(socket_cb->refcount==0)
     free(socket_cb);
 
 return 0;
}

if(socket_cb->type==l_socket) 
{
PORT_MAP[socket_cb->port]==NULL	;


kernel_broadcast(&socket_cb->listener_s.req_available);

if(socket_cb->refcount==0)   
 free(socket_cb);

 return 0;
}
if(socket_cb->type==p_socket)
  {
   if(pipe_writer_close(socket_cb->p_socket.write_pipe)!=0||pipe_reader_close(socket_cb->p_socket.read_pipe)!=0)
     {
     return -1;
     }
   else
  {
  if(socket_cb->refcount==0)
   free(socket_cb);
  return 0;

  }
  }
 return -1;

}
