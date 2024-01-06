#include "kernel_socket.c"
#include "tinyos.h"
#include "util.h"
#include "kernel_sched.h"
#include "kernel_dev.h"
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"



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
socket_cb* peer;
pipe_cb* write_pipe;
pipe_cb* read_pipe;
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
listener_socket listener_s;
unbound_socket  unbound_s;
peer_socket     peer_s;
     };
  
}Socket_CB;

