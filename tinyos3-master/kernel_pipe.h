#include "bios.h"
#include "util.h"
#include "kernel_cc.h"



/*Declarations regarding PIPES */
#define PIPE_BUFFER_SIZE 8192

typedef struct pipe_control_block {
  FCB *reader;
  FCB *writer;
  CondVar has_space;
  CondVar has_data;
  int w_position;
  int r_position;
  char BUFFER[PIPE_BUFFER_SIZE];
  int used_space;
  
}Pipe_CB;

/*Declaration of method for acquiring a new PipeCB*/
Pipe_CB* acquire_PipeCB();

/*Function Declarations*/
int sys_Pipe(pipe_t* pipe);

int pipe_write(void* pipecb_t, const char *buf, uint n);
int pipe_read(void* pipecb_t, char *buf, uint n);
int do_nothing(void* pipecb_t, char *buf, uint n);
int pipe_writer_close(void* _pipecb);
int pipe_reader_close(void* _pipecb);