/////////////////////////////////////////////////////////////////////////
//
// Programmers: Boris Salazar and Clayton Wong
//
// p1.c
//
// description: used to implement virtual memory.
//
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// header files
//
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase5.h>
#include <vm.h>
#include <mmu.h>
#include <proc.h>
#include <stdio.h>
#include <strings.h>
#include <frame.h>
#include <disk.h>
#include <map.h>
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// globals
//
extern int v_mem;
extern Process ProcTable[MAXPROC];
extern FTE* frameTable;
extern int numFrames;
extern void* VMR;
extern VmStats vmStats;
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
//
// p1_fork
//
// description: adds processes to the phase5 proccess table if
// the flag v_mem, is set.  The flag v_mem is set when the MMU is
// initialized, and virtual memory is turned on.  v_mem is unset when
// the virtual memory is turned off.
//
// returns: none
//
void
p1_fork(int pid)
{
	int rc;

	if (v_mem){
		//
		// if virtual memory is turned on then add the process to the
		// virtual memory process table
		//
		rc = addto_proctable(pid);
		if (rc < 0){
			fprintf(stderr, "p1_fork(): unable to add process to proc_table, rc=%d\n", rc);
		}
	}

//	console("in p1_fork(): pid = %d\n", pid);
}
//
// end p1_fork function
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
//
// p1_switch
//
// description: p1_switch is called whenever the dispatcher performs
// a context switch.  p1_switch unloads all of the mappings from the
// old process and loads all of the valid mappings for the new process.
// A valid mapping is one in which there is a frame for the page, if there
// is not a frame for the page, no mapping is performed for that page/frame.
// If a nonexistent page is references, an MMU interrupt will orccur and
// a frame should be allocated.
//
// returns: none
//
void
p1_switch(int old, int new)
{
	if (v_mem){
		//
		// keep track of the number of context switches
		//
		vmStats.switches++;

		//
		// unmap old
		//
		unmap(old);

		//
		// map new, (only the valid mappings)
		//
		map(new);
	} // end if (v_mem)
}
//
// end p1_switch function
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
//
// p1_quit
//
// description: cleans up the process, and re-initializes the process
// slot in the process table to be used by another process
//
void
p1_quit(int pid)
{
	int proc_slot;
	int i;
	PTE* pt;

	proc_slot = find_proc(pid);
	if (proc_slot >=0 ){
		//
		// remove all references to
		// the frame table
		//
		for (i=0; i < numFrames; i++){
			if (frameTable[i].owner_pid==pid){
				frameTable[i].owner_pid=NONE;
				frameTable[i].page_offset=NONE;
				frameTable[i].state=EMPTY;
			}
		}

		//
		// reinitialize the page table
		//
		pt=ProcTable[proc_slot].pageTable;
		for (i=0; i < ProcTable[proc_slot].numPages; i++){
			pt[i].state=EMPTY;
			pt[i].frame=NONE;
			pt[i].track=NONE;
			MMU_Unmap(TAG, i);
		}

		//
		// reinitialize the process slot
		//
		MboxRelease(ProcTable[proc_slot].p_mbox);
		bzero((void*)&ProcTable[proc_slot], sizeof(Process));
		ProcTable[proc_slot].pid=NONE;
		ProcTable[proc_slot].status=EMPTY;
		ProcTable[proc_slot].pageTable=pt;
		ProcTable[proc_slot].p_mbox=MboxCreate(1, 0);
	}
}
//
// end p1_quit function
/////////////////////////////////////////////////////////////////////////


