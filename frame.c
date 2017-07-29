//////////////////////////////////////////////////////////////
//
// programmers: Boris Salazar and Clayton Wong
//
// frame.c
//
// description: functions performed with frames and pages
// in the virtual memory system of USLOSS
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// header files
//
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <vm.h>
#include <strings.h>
#include <proc.h>
#include <mmu.h>
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// globals
extern int numFrames;
extern FTE* frameTable;
extern int frameTable_mutex;
extern int numFrames;
extern int clock_hand;
extern Process ProcTable[MAXPROC];
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// find_empty_frame
//
// description: iterates through the frame table and
// looks for a frame who's state is EMPTY, then sets
// that frames state to INUSE
//
// returns the index of the empty frame if one is found
// returns -1 if no empty frame was found
//
int find_empty_frame(void){
	int offset;

	/////////////////////////////////
	//
	// begin mutex
	//
	MboxSend(frameTable_mutex, NULL, 0);
	//
	//
	//

	//
	// iterate through the frame table
	// to find a slot which is EMPTY
	//
	for (offset=0; offset < numFrames; offset++){
		if (frameTable[offset].state==EMPTY){
			frameTable[offset].state=INUSE;
			break;
		}
	}

	//
	//
	//
	MboxReceive(frameTable_mutex, NULL, 0);
	//
	// end mutex
	//
	/////////////////////////////////

	if (offset == numFrames){
		//
		// no empty frames
		//
		return -1;
	} else {
		//
		// return index of the first frame
		// found which was empty
		//
		return offset;
	}
}
//
// end find_empty_frame function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// find_page
//
// description: iterates through the frame table and looks
// for the page_offset param that references any frame.
//
// returns the index in the frame table to which the given
//         page references
// returns -1 if there is not frame referenced by the given
//         page
//
int find_page(int page_offset, int fpid){
	int i;

	for (i=0; i < numFrames; i++){
		if (frameTable[i].page_offset==page_offset
		&&  frameTable[i].owner_pid==fpid){
			return i;
		}
	}

	return -1;
}
//
// end find_page function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// dump_frameTable
//
// description: prints out the frame table
//
void dump_frameTable(){
	int i;
	int access;
	char str_state[10];
	char str_access[10];


	printf("frame\tstate\towner\tref by page\taccess\n");

	for (i=0; i < numFrames; i++){
		switch(frameTable[i].state){
			case EMPTY: strcpy(str_state, "EMPTY");
				break;
			case INUSE: strcpy(str_state, "INUSE");
				break;
		}

		MMU_GetAccess(i, &access);
		switch(access){
			case MMU_REF: strcpy(str_access, "MMU_REF");
				break;
			case MMU_DIRTY: strcpy(str_access, "MMU_DIRTY");
				break;
			case MMU_REF | MMU_DIRTY: strcpy(str_access, "DIRTY & REF");
				break;
			default: sprintf(str_access, "%d", access);
				break;
		}

		printf("   %d\t%s\t%d\t\t%d\t%s\n",
			i, str_state, frameTable[i].owner_pid, frameTable[i].page_offset, str_access);
	}
}
//
// end dump_frameTable function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// find_dump_frame
//
// description: implements a clock algorithm
//
// "When a page fault occurs, the frame being pointed
// to by clock_hand is inspected.  If its reference bit is 0,
// the frame is evicted, the new frame is inserted into the clock
// in its place If the reference bit is 1, then it is cleared
// and the clock_hand is advanced to the next page.  This process
// is repeated until a page is found without its reference bit set"
// (Tanenbaum, 218).
//
// MMU_REF   = 0x1 = 0001
// MMU_DIRTY = 0x2 = 0010
//
int find_dump_frame(){
	int rc;
	int access;

	while (1){
		if (clock_hand==numFrames){
			//
			// wrap the clock hand around
			//
			clock_hand=0;
		}

		rc = MMU_GetAccess(clock_hand, &access);
		if (rc != MMU_OK){
			fprintf(stderr,
				"find_dump_frame(): call to MMU_GetAccess returned %d\n", rc);
			return -1;
		}
		if (access & MMU_REF){
			//
			// remove ref access bit, and continue
			//
			rc=MMU_SetAccess(clock_hand, access & MMU_DIRTY);
			if (rc < 0){
				fprintf(stderr,
					"find_dump_frame(): call to MMU_SetAccess returned %d\n", rc);
				return -1;
			}
		} else {
			//
			// set referenced bit for frame
			//
			if (access == MMU_DIRTY){
				MMU_SetAccess(clock_hand, MMU_DIRTY | MMU_REF);
				if (rc < 0){
					fprintf(stderr,
						"find_dump_frame(): call to MMU_SetAccess returned %d\n", rc);
					return -1;
				}
			} else {
				MMU_SetAccess(clock_hand, MMU_REF);
				if (rc < 0){
					fprintf(stderr,
						"find_dump_frame(): call to MMU_SetAccess returned %d\n", rc);
					return -1;
				}
			}
			//
			// exit the loop
			//
			break;
		}
		clock_hand++;

	} // end while loop

	return clock_hand;
}
//
// end find_dump_frame function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// free_frame_count
//
int free_frame_count(void){
	int frame;
	int free_frames;

	free_frames=0;
	for (frame=0; frame < numFrames; frame++){
		if (frameTable[frame].state==EMPTY){
			free_frames++;
		}
	}

	return free_frames;
}
//
// end free_frame_count function
//////////////////////////////////////////////////////////////

