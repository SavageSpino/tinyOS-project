
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"


/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;

  rlnode_init(& pcb->ptcb_list, NULL);
  pcb->thread_count = 0;
}


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}


PTCB* acquire_PTCB(TCB* tcb, Task task, int argl, void* args)
{
  PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB));  /*Allocate memory space for the PTCB*/
  assert(ptcb != NULL);                       /*Failsafe to make sure address space is empty*/

  /*Initialize PTCB*/
  ptcb->tcb = tcb;                            /*Connection from PTCB to TCB*/
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exit_cv = COND_INIT;
  ptcb->refcount = 0;
  rlnode_init(&ptcb->ptcb_list_node, ptcb);

  tcb->owner_ptcb = ptcb;                     /*Connection from TCB to PTCB*/
  ptcb->refcount++;                           /*With TCB connection complete adjust the refcount*/

  return ptcb;                                /*Return poiinter to first memory location of the PTCB*/
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */

/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}


/*
	System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
  if(call != NULL) {
    newproc->main_thread = spawn_thread(newproc, start_main_thread);

    /* create main PTCB through aqcuire_PTCB*/
    PTCB* main_PTCB = acquire_PTCB(newproc->main_thread, newproc->main_task, newproc->argl, newproc->args);

    newproc->main_thread->owner_ptcb = main_PTCB;                     /*Connection from TCB to PTCB*/
    main_PTCB->refcount++;
    rlist_push_back(& newproc->ptcb_list, &main_PTCB->ptcb_list_node);

    newproc->thread_count++;

    /* wake up TCB */
    wakeup(newproc->main_thread);
  }
  


finish:
  return get_pid(newproc);
}


/* System call */
Pid_t sys_GetPid()
{
  return get_pid(CURPROC);
}


Pid_t sys_GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    kernel_wait(& parent->child_exit, SCHED_USER);
  
  cleanup_zombie(child, status);
  
finish:
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  int no_children, has_exited;
  while(1) {
    no_children = is_rlist_empty(& parent->children_list);
    if( no_children ) break;

    has_exited = ! is_rlist_empty(& parent->exited_list);
    if( has_exited ) break;

    kernel_wait(& parent->child_exit, SCHED_USER);    
  }

  if(no_children)
    return NOPROC;

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

  return cpid;
}


Pid_t sys_WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void sys_Exit(int exitval)
{

  PCB *curproc = CURPROC;  /* cache for efficiency */
  

  /* First, store the exit status */
  curproc->exitval = exitval;

  /* 
    Here, we must check that we are not the init task. 
    If we are, we must wait until all child processes exit. 
   */
  if(get_pid(curproc)==1) {
  
    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
 
  } 
  curproc->exitval=exitval;
  
  sys_ThreadExit(exitval); 
  
 } 

/*============================= SYS INFO CODE ======================================*/

static file_ops procinfo_ops = {

  .Open = (void*)procinfo_open,
  .Read = procinfo_read,
  .Write = (void *)procinfo_write,
  .Close = procinfo_close
};



Fid_t sys_OpenInfo()
{
  FCB* fcb;
  Fid_t fid;

  int result = FCB_reserve(1, &fid, &fcb);

  if(result == 0)
  {
    return NOFILE;
  }

  PICB* picb = (PICB*)xmalloc(sizeof(PICB));
  picb->cursor = 0;

  /*Connect fcb with the procinfo_control_block and with the procinfo ops*/
  fcb->streamobj = picb;
  fcb->streamfunc = &procinfo_ops;


  return fid;
}


int procinfo_open(void* pi_cb, char* buf, uint size)
{
  return NOFILE;
}

int procinfo_read(void* pi_cb, char* buf, uint size)
{
  PICB* picb = (PICB*)pi_cb;


  while(PT[picb->cursor].pstate == FREE && picb->cursor < MAX_PROC)
  {
     picb->cursor++;
  }
  if(picb->cursor == MAX_PROC)
  {
    return 0;
  }

  picb->info.pid = get_pid(&PT[picb->cursor]);
  picb->info.ppid = get_pid(PT[picb->cursor].parent);
  if(PT[picb->cursor].pstate == ZOMBIE)
  {
    picb->info.alive = 0;
  }else{
     picb->info.alive = 1;
  }
  picb->info.thread_count = PT[picb->cursor].thread_count;
  picb->info.main_task = PT[picb->cursor].main_task;
  picb->info.argl = PT[picb->cursor].argl;
      
  int dataLen = PT[picb->cursor].argl;
  if(dataLen > PROCINFO_MAX_ARGS_SIZE)
  {
     dataLen = PROCINFO_MAX_ARGS_SIZE;
  }

  memcpy(picb->info.args, PT[picb->cursor].args, dataLen);
  memcpy(buf, (char *)&picb->info,sizeof(procinfo));
  picb->cursor++;

  return(sizeof(procinfo));
}

int procinfo_write(void* pi_cb, char* buf, uint size)
{
  return NOFILE;
}

int procinfo_close(void* pi_cb)
{
  PICB* picb = (PICB*)pi_cb;
  free(picb);
  return 0;
}
