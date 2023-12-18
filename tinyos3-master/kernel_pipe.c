
#include "tinyos.h"
#include "kernel_dev.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

/*Initializing file_ops for reader and writer operations*/
static file_ops reader_file_ops = {

	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_reader_close
};

static file_ops writer_file_ops = {
	
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_writer_close
};

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

	int available_space = 0;
	int used_space = 0;

	if(pipeCB->w_position > pipeCB->r_position)     /*pipe is in the form of [||||||||||||||||.................] or [........||||||||||||||........] or [.......................]*/
	{
		/*Operations to figure out how much space is available*/
		used_space = pipeCB->w_position - pipeCB->r_position;
		available_space = PIPE_BUFFER_SIZE - used_space;

		if(available_space >= n)
		{
			int counter = 0;
			/*Loop to transfer all the data*/
			while(counter<n)
			{
				pipeCB->BUFFER[pipeCB->w_position] = buf[counter];	/*Move data from one buffer to the other*/

				/*Check if w_position is at the end of the buffer, in which case reset it to 0*/
				if(pipeCB->w_position == PIPE_BUFFER_SIZE-1)
				{
					pipeCB->w_position = 0;
				}else{
					pipeCB->w_position++;
				}

				counter++;
			}

			kernel_broadcast(&pipeCB->has_data);
			return n;

		}else{
			while(pipeCB->reader != NULL)	/*While someone is reading the pipe, wait to read and remove data, in order to make space*/
			{
				kernel_wait(&pipeCB->has_space, SCHED_PIPE);
			}
		}
	}else{			/*pipe is in the form of [|||||||||||....................||||||||||||||||]*/

		/*Operations to figure out how much space is available*/
		used_space = (PIPE_BUFFER_SIZE - pipeCB->r_position-1) + pipeCB->w_position; 
		available_space = PIPE_BUFFER_SIZE - used_space;

		if(available_space >= n)
		{
			int counter = 0;
			/*Loop to transfer all the data*/
			while(counter<n)
			{
				pipeCB->BUFFER[pipeCB->w_position] = buf[counter];	/*Move data from one buffer to the other*/

				/*Check if w_position is at the end of the buffer, in which case reset it to 0*/
				if(pipeCB->w_position == PIPE_BUFFER_SIZE-1)
				{
					pipeCB->w_position = 0;
				}else{
					pipeCB->w_position++;
				}

				counter++;
			}

			kernel_broadcast(&pipeCB->has_data);
			return n;

		}else{
			while(pipeCB->reader != NULL)	/*While someone is reading the pipe, wait to read and remove data, in order to make space*/
			{
				kernel_wait(&pipeCB->has_space, SCHED_PIPE);
			}
		}

	}

}


int pipe_read(void* pipecb_t, char *buf, uint n)
{
	return -1;
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
		return 0;

	}else{
		Cond_Broadcast(&pipeCB->has_data);
		return 0;
	}

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
	
	/*No check needed, if there is no one to read data, no one should enter new data*/
	free(pipeCB);
	return 0;
}
