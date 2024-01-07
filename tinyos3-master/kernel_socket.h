#include "tinyos.h"
#include "util.h"
#include "kernel_dev.h"
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "kernel_pipe.h"
#include "bios.h"



typedef enum{
  SOCKET_LISTENER,
  SOCKET_UNBOUND,
  SOCKET_PEER  

}socket_type;

typedef struct listener_socket{
  rlnode queue;
  CondVar req_available;

}l_socket;

typedef struct peer_socket{
  Socket_CB* peer;
  Pipe_CB* write_pipe;
  Pipe_CB* read_pipe;

}p_socket;


typedef struct unbound_socket{
  rlnode u_socket;

}u_socket;


typedef struct socket_control_block {
  uint refcount;
  FCB* fcb; 
  socket_type type;
  port_t port;
  union{
    l_socket listener_s;
    u_socket  unbound_s;
    p_socket     peer_s;
  };
  
}Socket_CB;

int socket_open();
int socket_read();
int socket_write();
int socket_close();

