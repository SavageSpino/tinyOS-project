
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


/*The following function is based on start_main_thread and is used to initialise and run the thread*/
void thread_Exe()
{
  TCB* mainTCB = cur_thread(); /*Pointer to the TCB of the hread that initialy called sys_CreateThread*/

  int exitval;
  Task call = mainTCB->owner_ptcb->task;
  int argl = mainTCB->owner_ptcb->argl;
  void* args = mainTCB->owner_ptcb->args;

  exitval = call(argl, args);
  ThreadExit(exitval);
}



/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PCB* curPCB = CURPROC;  /*curPCB is a pointer to the PCB of the current process*/

  TCB* newTCB = spawn_thread(curPCB, thread_Exe); /*Pointer to the  newly created and initialised TCB*/
  assert(newTCB != NULL);
  newTCB->owner_pcb = curPCB;                     /*Connection from TCB to PCB*/

  PTCB* newPTCB = acquire_PTCB(newTCB, task, argl, args);  /*Pointer to the memory location of the new PTCB returned by acquire_PTCB()*/

  rlist_push_back(&curPCB->ptcb_list, &newPTCB->ptcb_list_node);  /*Add the newPTCB's list node to the back of the ptcb list of the PCB*/

  wakeup(newTCB);    /*Change state of newTCB to READY and add to scheduler que*/
  return (Tid_t)newPTCB;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->owner_ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
 PCB *curproc = CURPROC;  /* cache for efficiency */

  /* 
    Do all the other cleanup we want here, close files etc. 
   */

  /* Release the args data */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }





    /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    if(curproc->parent !=NULL){
    rlist_push_front(& curproc->parent->e xited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
                              }
  

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));




  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
 

 /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);


}

