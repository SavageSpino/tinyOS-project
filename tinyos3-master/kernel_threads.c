
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"


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
  curPCB->thread_count++;   /*Increase thread count to include the thread just created*/

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
	PCB* pcb = CURPROC; /*Get the pcb of the current process*/

  PTCB* ptcb = (PTCB*)tid;

  /*Check if tid does not exist*/
  if(ptcb == NULL)
  {
    return -1;
  }

  /*Check if tid coresponds to a ptcb of the same process*/
  if(!rlist_find(&pcb->ptcb_list,ptcb,NULL))
  {
    return -1;
  }

  /*Check if the id of the thread entered is its own*/
  if(tid == sys_ThreadSelf())
  {
    return -1;
  }

  /*Check if the thread to be Joined is legal*/
  if(ptcb->detached == 1)
  {
    return -1;
  }
  
  /*Since it is legal to join the thread increase refcount*/
  ptcb->refcount++;

  /*While thread to be joined is still running, wait for the thread to exit and return its exit_cv*/
  while(ptcb->exited == 0 && ptcb->detached == 0)
  {
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }

  /*Now that thread to be joined has exited, reduce refcount*/
  ptcb->refcount--;

  if(ptcb->detached == 1)
  {
    return -1;
  }

  /*If exitval is valid store it in PTCB*/
  if(exitval != NULL)
  {
    (*exitval) = ptcb->exitval;
  }

  /*Check if the refcount of the joined thread is 0, if it is, the PTCB serves no purpose anymore*/
  if(ptcb->refcount == 0)
  {
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }

  return 0; /*Return value to signal success*/
}


/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PCB* pcb = CURPROC; /*Get the pcb of the current process*/

  rlnode* PTCB_node = rlist_find(&pcb->ptcb_list, (PTCB*)tid, NULL);

  /*Checks if PCB has a PTCB in its rlist with the tid entered to detach*/
  if(PTCB_node == NULL)
  {
    return -1;
  }

  PTCB* ptcb = PTCB_node->ptcb; /*PTCB with the correct id found*/

  /*Check if PTCB that needs to be detached is legal*/
  if(ptcb == NULL || ptcb->exited == 1)
  {
    return -1;
  }

  ptcb->detached = 1; /*Change flag*/
  
  kernel_broadcast(&ptcb->exit_cv);   /*Broadcast to wakeup other threads since the one which called sys_ThreadDetach is no longer running*/
  
  return 0; /*Return value to signal success*/

}





/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
 PCB *curproc = CURPROC;  /* cache for efficiency */
 TCB *curthd=cur_thread();
 PTCB *CURptcb= curthd->owner_ptcb;

 CURptcb->exitval=exitval;

 
 curproc->thread_count=curproc->thread_count-1;
 kernel_broadcast(&(CURptcb->exit_cv));
 CURptcb->refcount=CURptcb->refcount-1;
 CURptcb->exited=1;
 kernel_broadcast(&CURptcb->exit_cv);
 if(curproc->thread_count==0)
 {
  if (get_pid(curproc)!= 1) {
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
      rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(& curproc->parent->child_exit);
                              }
  

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));

 


  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
 
  }
 /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);


}

