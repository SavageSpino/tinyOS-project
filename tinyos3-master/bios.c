#include "kernel_pipe.h"

#include "kernel_dev.h"

#include "kernel_streams.h"

#include "kernel_sched.h"

#include "kernel_cc.h"



/*'

==================== TO DO ==============================

Add used_space in pipe_control_block -----DONE-----

Make a pipe.h file and move the declarations from tinyos.h there -----DONE-----

Make the do_nothing function for the file_ops -----DONE-----

Change the pipe_Write logic ----DONE----

Make it so that when there is space in the buffer but not enought ot fit the entire new data, so that you insert as much as it fits ----DONE----

Change pipe_reader_close to match the logic of pipe_writer_close ----DONE----



Fix Thread Exit to do a proper ptcb cleanup ----DONE----

*/











/*Initializing file_ops for reader and writer operations*/

static file_ops reader_file_ops = {



.Open = (void *)do_nothing,

.Read = pipe_read,

.Write = (void *)do_nothing,

.Close = pipe_reader_close

};



static file_ops writer_file_ops = {



.Open = (void *)do_nothing,

.Read = do_nothing,

.Write = pipe_write,

.Close = pipe_writer_close

};



int do_nothing(void* pipecb_t, char *buf, uint n)

{

return -1;

}



Pipe_CB* acquire_PipeCB()

{

/*Reserve memory space for the pipe control block*/

Pipe_CB* pipeCB;

pipeCB = (Pipe_CB*)xmalloc(sizeof(Pipe_CB));



/*Initializing the variables of the pipe control block*/

pipeCB->has_data = COND_INIT;

pipeCB->has_space = COND_INIT;

pipeCB->r_position = 0;

pipeCB->w_position = 0;

pipeCB->used_space = 0;



return pipeCB;

}





int sys_Pipe(pipe_t* pipe)

{

Fid_t fids[2];

FCB* fcbs[2];



int ret_val = FCB_reserve(2, fids, fcbs);



if(ret_val == 1)

{

/*Connecting the reserved FIDTs to the reader and writer ends of the pipe*/

pipe->read = fids[0];

pipe->write = fids[1];



/*Acquiring a pipeCB and connecting the reader and writier to their corresponding FCB, reserved prior*/

Pipe_CB* pipeCB = acquire_PipeCB();

pipeCB->reader = fcbs[0];

pipeCB->writer = fcbs[1];



/*Connecting the FCBs stream objects to the the pipe control block*/

pipeCB->reader->streamobj = pipeCB;

pipeCB->writer->streamobj = pipeCB;



/*Connecting the FCBs strea functions to the correct file operation*/

pipeCB->reader->streamfunc = &reader_file_ops;

pipeCB->writer->streamfunc = &writer_file_ops;



return 0;



}else{

return -1;

}



}



int pipe_write(void* pipecb_t, const char *buf, uint n)

{

Pipe_CB* pipeCB = (Pipe_CB*)pipecb_t;



/*Check if pipe control block entered exists*/

if(pipeCB == NULL)

{

return -1;

}



/*Check if reader and writer are valid*/

if(pipeCB->reader == NULL || pipeCB->writer == NULL)

{

return -1;

}



/*Check if the buffer is full*/

while(pipeCB->used_space == PIPE_BUFFER_SIZE && pipeCB->reader != NULL) /*While someone is reading the pipe, wait to read and remove data, in order to make space*/

{

kernel_wait(&pipeCB->has_space, SCHED_PIPE);

}



int available_space = PIPE_BUFFER_SIZE - pipeCB->used_space;

/*Check if space is sufficient to fit all the data, if not enter as much as it fits*/

if(available_space < n)

{

n = available_space;

}



int counter = 0;

/*Loop to transfer all the data*/

while(counter<n)

{

pipeCB->BUFFER[pipeCB->w_position] = buf[counter]; /*Move data from one buffer to the other*/



/*Check if w_position is at the end of the buffer, in which case reset it to 0*/

if(pipeCB->w_position == PIPE_BUFFER_SIZE-1)

{

pipeCB->w_position = 0;

}else{

pipeCB->w_position++;

}



counter++;

pipeCB->used_space++;

}

//pipeCB->w_position = (pipeCB->w_position+1)%PIPE_BUFFER_SIZE



kernel_broadcast(&pipeCB->has_data);

return n;

}





int pipe_read(void* pipecb_t, char *buf, uint n)

{

Pipe_CB* pipeCB = (Pipe_CB*)pipecb_t;

/*Check if pipe control block entered exists*/

if(pipeCB == NULL)

{

return -1;

}



/*Check if reader is valid*/

if(pipeCB->reader == NULL)

{

return -1;

}



/*While the buffer is empty and the pipe writer is not closed, wait for data to be entered*/

while(pipeCB->used_space == 0 && pipeCB->writer != NULL)

{

kernel_wait(&pipeCB->has_data, SCHED_PIPE);

}



uint  commitedSpace=n;

/*Adjust the length of the data to read based on the pipeCB's used space*/

if(commitedSpace > pipeCB->used_space)

{

commitedSpace = pipeCB->used_space;

}



int counter = 0;

/*Loop to read all the data*/

while(counter<commitedSpace)

{

/*Check if there are no more data to read*/

if(pipeCB->used_space != 0)

{

buf[counter] = pipeCB->BUFFER[pipeCB->r_position] ; /*Move data from one buffer to the other*/

/*Check if w\r_position is at the end of the buffer, in which case reset it to 0*/

if(pipeCB->r_position == PIPE_BUFFER_SIZE-1)

{

pipeCB->r_position = 0;

}

else{

pipeCB->r_position++;

}

counter++;

pipeCB->used_space--;

}else{

break;

}

}

kernel_broadcast(&pipeCB->has_space);

return counter;



}





int pipe_writer_close(void* _pipecb)

{

Pipe_CB* pipeCB = (Pipe_CB*)_pipecb;



/*Checking if pipe control block entered is valid/exists*/

if(pipeCB == NULL)

{

return -1;

}



pipeCB->writer = NULL;



if(pipeCB->reader == NULL)

{

free(pipeCB);



}else{

kernel_broadcast(&pipeCB->has_data);

}



return 0;



}



int pipe_reader_close(void* _pipecb)

{



Pipe_CB* pipeCB = (Pipe_CB*)_pipecb;



/*Checking if pipe control block entered is valid/exists*/

if(pipeCB == NULL)

{

return -1;

}



pipeCB->reader = NULL;



if(pipeCB->writer == NULL)

{

free(pipeCB);



}else{
kernel_broadcast(&pipeCB->has_space);

}



return 0;

}