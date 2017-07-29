/////////////////////////////////////////////////////////////////
//
// Programmers: Boris Salazar and Clayton Wong
//
// map.c
//
// description: functions which map and unmap pages to frame
// for use with virtual memory
//
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
// header files
//
#include <phase1.h>
#include <vm.h>
#include <proc.h>
#include <mmu.h>
#include <strings.h>
#include <stdio.h>
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
// globals
//
extern Process ProcTable[MAXPROC];
extern FTE* frameTable;
extern void* VMR;
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//
// map
//
// description: performs the mapping of pages to frame for the
// given process, and initializes frame contents and access
// when the page is first referenced
//
void map(int pid){

	PTE* pt;
	int new_proc_slot;
	int page_index;
	int frame_index;
	int rc;

	new_proc_slot = find_proc(pid);
	if (new_proc_slot >= 0){
		pt=ProcTable[new_proc_slot].pageTable;
		for (page_index=0; page_index < ProcTable[new_proc_slot].numPages; page_index++){
			if (pt[page_index].state!=EMPTY){
				//
				// see if the page references a valid frame, a valid frame is
				// one which is owned by the new process and referenced from
				// the processes page table entry, and whose state is not NONE
				//
				frame_index = pt[page_index].frame;

				if ((frame_index != NONE || pt[page_index].track != NONE)
				&&	frameTable[frame_index].state != EMPTY
				&&  frameTable[frame_index].owner_pid==pid
				&&  frameTable[frame_index].page_offset==page_index){
					//
					// map the valid page/frame mapping
					//
					MMU_Map(TAG, page_index, frame_index, MMU_PROT_RW);

					if (pt[page_index].state == FIRST_TIME) {
						bzero(VMR + (page_index * MMU_PageSize()), MMU_PageSize());
						pt[page_index].state = INUSE;

						//
						// init. access to frame to 0
						//
						rc=MMU_SetAccess(frame_index, 0);
						if (rc < 0){
							fprintf(stderr, "p1_switch(): call to MMU_SetAccess returned %d\n", rc);
						}
					}
				}
			}
		} // end for loop
	} // end if (new_proc_slot >=0)
}
//
// end map function
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//
// unmap
//
// description: unmaps all pages which have state INUSE from their
// frames for the given process
//
void unmap(int pid){

	PTE* pt;
	int old_proc_slot;
	int page_index;

	old_proc_slot = find_proc(pid);
	if (old_proc_slot >= 0){
		pt=ProcTable[old_proc_slot].pageTable;
		for (page_index=0; page_index < ProcTable[old_proc_slot].numPages; page_index++){
			if (pt[page_index].state==INUSE){
				//
				// unmap all pages that are INUSE
				//
				MMU_Unmap(TAG, page_index);
			}
		}
	}
}
//
// end unmap function
/////////////////////////////////////////////////////////////////
