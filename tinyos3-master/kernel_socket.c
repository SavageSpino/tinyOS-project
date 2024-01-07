
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
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	if(sock < 0 || sock > MAX_FILEID)
	{
		return -1;
	}

	FCB* fcb = get_fcb(sock);
	if(fcb == NULL)
	{
		return -1;
	}

	Socket_CB* socket_cb = (Socket_CB *)fcb->streamobj;
	if(socket_cb == NULL)
	{
		return -1;
	}

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
