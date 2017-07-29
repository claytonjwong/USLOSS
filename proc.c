//////////////////////////////////////////////////////////////////////
//
// programmer: Clayton Wong
//
// proc.c
//
// description: functions to keep track of processes
// in the process table
//
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// header files
//
#include <stdio.h>
#include <stdlib.h>
#include <vm.h>
#include <phase1.h>
#include <phase2.h>
#include <libuser.h>
#include <strings.h>
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// globals
//
extern Process ProcTable[MAXPROC];
extern int ProcTable_mutex;
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// find_proc
//
// description: given the process id, this function returns the process's
// index in the process table (aka proc_slot)
//
int find_proc(int pid){
	int proc_slot=0;

	do{
		if (ProcTable[proc_slot].pid == pid){
			if (pid == -1){
				if(ProcTable[proc_slot].status == INUSE){
					break;
				}
			} else {
				break;
			}
		}
		proc_slot++;
	}while(proc_slot < MAXPROC);

	if (proc_slot == MAXPROC){
		return -1;
	}

	return proc_slot;
}
//
// end find_proc function
//////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// addto_proctable
//
// description: adds process with given pid to the process table
// by finding an empty slot, and initializing its pid to given pid
// and setting its status to INUSE
//
int addto_proctable(int pid){
	int child_slot;

	//////////////////////////
	// begin critical section
	//
	MboxSend(ProcTable_mutex, NULL, 0);
	//
	//
	//

	//
	// find an empty process table slot
	//
	child_slot=0;
	do{
		if (ProcTable[child_slot].status==EMPTY){
			break;
		}
		child_slot++;
	}while(child_slot < MAXPROC);

	if (child_slot == MAXPROC){
		//
		// there is no room for the process
		//
		return -1;
	}

	//
	// intialize attributes
	//
	ProcTable[child_slot].status = INUSE;
	ProcTable[child_slot].pid = pid;
	//
	//
	//
	MboxReceive(ProcTable_mutex, NULL, 0);
	//
	// end critical section
	////////////////////////

	return child_slot;
}
//
// end addto_proctable
///////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// init_proc
//
// description: initializes the process in the process table at index
// "index", by first zeroing it out, setting its pid to -1, status to
// EMPTY, and initializing its private semaphore to count of 0
//
void init_proc(int index){
	int rc;

	bzero(&ProcTable[index], sizeof(Process));
	ProcTable[index].pid = -1;
	ProcTable[index].status = EMPTY;
	rc = Mbox_Create(1, sizeof(int), &ProcTable[index].p_mbox);
	if (rc < 0){
		fprintf(stderr, "init_proc(): unable to create mailbox, rc=%d\n", rc);
	}
	ProcTable[index].pagerBuf = (void *)malloc(PAGE_SIZE);
}
//
// end init_proc function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// dump_vm_processes
//
void dump_vm_processes(void){
	int i;

	printf("PID\t\tStatus\n");

	for (i=0; i < MAXPROC; i++){
		if (ProcTable[i].status != EMPTY){
			printf("%d\t%d\n", ProcTable[i].pid, ProcTable[i].status);
		}
	}
}
//
// end dump_vm_processes function
//////////////////////////////////////////////////////////////////////
