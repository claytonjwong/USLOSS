/////////////////////////////////////////////////////////////////////////
//
// Programmers: Boris Salazar and Clayton Wong
//
// vm.c
//
// description: implementation of virtual memory for USLOSS
//
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// header files
//
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <strings.h>
#include <mailbox.h>
#include <proc.h>
#include <frame.h>
#include <disk.h>
#include <provided_prototypes.h>
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// testcases
//
/*
simple1   -- OK
simple2   -- OK
simple3   -- OK
simple4   -- OK
simple5   -- OK    freeBlocks=25
test1     -- OK
test2     -- OK    freeBlocks=16
test3     -- OK    freeBlocks=30, pageIns=4
test4     -- OK    freeBlocks=24
gen       -- OK
replace1  -- OK
replace2  -- OK    freeBlocks=28
outOfSwap -- OK    freeBlocks=0
clock     -- OK
quit      -- OK    freeBlocks=30
*/
/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// globals
//
Process ProcTable[MAXPROC];
int ProcTable_mutex;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */

FTE* frameTable=NULL;
int numFrames=0;
int frameTable_mutex;
int disk_full=FALSE;
int clock_hand=NONE;

static int fault_mbox;

VmStats  vmStats;

void* VMR;
int v_mem;

extern int start5(char*);

//void * pagerBuf[MAXPAGERS];

int PagerZapFlag;

/////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////
// function declarations
//
static void kernel_vm_init(sysargs *sa);
static void kernel_vm_cleanup(sysargs *sa);
static void* vm_init(int mappings, int pages, int frames, int daemons);
static void vm_cleanup(void);
static void FaultHandler(int type, int offset);
static int Pager(char *buf);
static void PrintStats(void);
/////////////////////////////////////////////////////////////////////////

/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
start4(char *arg){
	int pid;
	int result;
	int status;
	int rc;
	int i;

	//
  // to get user-process access to mailbox functions
  // via system calls through the system call vector
  //
  sys_vec[SYS_MBOXCREATE]      = kernel_mbox_create;
  sys_vec[SYS_MBOXRELEASE]     = kernel_mbox_release;
  sys_vec[SYS_MBOXSEND]        = kernel_mbox_send;
  sys_vec[SYS_MBOXRECEIVE]     = kernel_mbox_recv;
  sys_vec[SYS_MBOXCONDSEND]    = kernel_mbox_condsend;
  sys_vec[SYS_MBOXCONDRECEIVE] = kernel_mbox_condrecv;
	//
	// user-process access to VM functions
	//
	sys_vec[SYS_VMINIT]    = kernel_vm_init;
	sys_vec[SYS_VMCLEANUP] = kernel_vm_cleanup;

	//
	// create fault mailbox, this is a queue used to
	// store faults, pagers block on a recv to this
	// mailbox, while the fault hander sends to this mailbox
	//
	rc=Mbox_Create(MAXPROC, sizeof(int), &fault_mbox);
	if (rc < 0){
		fprintf(stderr, "start4(): unable to create mailbox\n");
		return -1;
	}

	//
	// initialize pager zap flag to FALSE
	//
	PagerZapFlag =FALSE;

	//
	// initialize the process table by creating
	// a semaphore used for mutual exclusion,
	// and initializing each element in the table
	//
	rc=SemCreate(1, &ProcTable_mutex);
	if (rc < 0){
		fprintf(stderr, "start4(): unable to create semaphore, rc=%d\n", rc);
		return -1;
	}
	for (i=0; i < MAXPROC; i++){
		init_proc(i);
	}

	//
	// spawn off process start5,
	// and wait for it to terminate
	//
	result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
	if (result != 0) {
		console("start4(): Error spawning start5\n");
		Terminate(1);
	}
	result = Wait(&pid, &status);
	if (result != 0) {
		console("start4(): Error waiting for start5\n");
		Terminate(1);
	}

	Terminate(0);
	return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInitStub --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
kernel_vm_init(sysargs *sa)
{
	CheckMode();
	int mappings=(int)sa->arg1;
	int pages=(int)sa->arg2;
	int frames=(int)sa->arg3;
	int daemons=(int)sa->arg4;

	if (mappings <= 0
	||  pages <= 0
	||  frames <= 0
	||  daemons <=0){
		sa->arg4=(void*)-1;
	} else {
		VMR=vm_init(mappings, pages, frames, daemons);
		sa->arg1=VMR;
		sa->arg4=(void*)0;
	}
} /* VmInitStub */


/*
 *----------------------------------------------------------------------
 *
 * VmCleanupStub --
 *
 * Stub for the VmCleanup system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
kernel_vm_cleanup(sysargs *sa)
{
   CheckMode();
   vm_cleanup();
} /* VmCleanupStub */


/*
 *----------------------------------------------------------------------
 *
 * vm_init --
 *
 * Called by VmInitStub.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
static void *
vm_init(int mappings, int pages, int frames, int daemons)
{
	int status;
	int dummy;
	int i;
	int j;
	int rc;

	CheckMode();
	status = MMU_Init(mappings, pages, frames);
	if (status != MMU_OK) {
		console("vm_init: couldn't initialize MMU, status %d\n", status);
		abort();
	}

	//
	// setup the page fault handler
	//
  int_vec[MMU_INT] = FaultHandler;

  //
  // create the disk bitmap
  //
  rc = create_bitmap(SWAP_DISK);
  if (rc < 0){
		fprintf(stderr, "Unable to initialize disk unit %d, rc = %d\n", SWAP_DISK, rc);
	}

	//
	// create the page tables
	//
	for (i=0; i < MAXPROC; i++){
		//
		// allocate space for "pages" page table entries
		//
		ProcTable[i].pageTable=(PTE*)calloc(pages, sizeof(PTE));
		if (ProcTable[i].pageTable==NULL){
			fprintf(stderr, "vm_init(): call to calloc returned NULL\n");
			exit(1);
		}
		//
		// store size of the page table
		//
		ProcTable[i].numPages=pages;

		//
		// intitialize each page table entry's frame and track
		// to NONE, which means that the page is not stored
		// in a frame or on a disk
		//
		for (j=0; j < pages; j++){
			ProcTable[i].pageTable[j].state=EMPTY;
			ProcTable[i].pageTable[j].frame=NONE;
			ProcTable[i].pageTable[j].track=NONE;
		}
	}

	//
	// create the frame table by allocating
	// "frames" amount of FTE entries
	//
	frameTable = (FTE*)calloc(frames, sizeof(FTE));
	if (frameTable==NULL){
		fprintf(stderr, "vm_init(): call to calloc returned NULL\n");
		exit(1);
	}

	//
	// initialize the frame table by creating a mailbox
	// for mutual exclusion, and initializing each frame
	//
	numFrames = frames;
	frameTable_mutex = MboxCreate(1, 0);
	for (i=0; i < frames; i++){
		frameTable[i].owner_pid=NONE;
		frameTable[i].page_offset=NONE;
		frameTable[i].state=EMPTY;
		MMU_SetAccess(i, 0);
	}

	//
	// create the pager daemons
	//
	for (i=0; i < daemons; i++){
		fork1("Pager", Pager, NULL, 4*USLOSS_MIN_STACK, DAEMON_PRIORITY);
	}

	//
	// Initialize other vmStats fields.
	//
	memset((char *) &vmStats, 0, sizeof(VmStats));
	vmStats.pages = pages;
	vmStats.frames = frames;
	vmStats.blocks = disk_blocks(SWAP_DISK);

	//
	// set v_mem flag, this allows p1_fork,
	// and p1_switch to know that virtual
	// memory is running
	//
	v_mem=TRUE;

	//
	// return the virtual memory region address
	//
	return MMU_Region(&dummy);
} /* vm_init */


/*
 *----------------------------------------------------------------------
 *
 * vm_cleanup --
 *
 * Called by VmCleanupStub.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
static void
vm_cleanup(void)
{
	int i;
	int rc;
	CheckMode();

	//
	// unset v_mem flag, this allows p1_fork
	// and p1_switch to know that virtual
	// memory is no longer running
	//
  v_mem=FALSE;

	//
	// Set global Zap flag so all other Pagers stop working.
	//
	PagerZapFlag =TRUE;

	//
	// Kill the pagers
	//
	for (i=0; i < MAXPROC; i++){
		if (ProcTable[i].status==DAEMON){
			//
			// free up the pager daemon by sending
			// to the mailbox which it may be blocked
			// trying to receive on
			//
			rc=MboxCondSend(fault_mbox, NULL, 0);
			if (rc < 0){
				fprintf(stderr, "vm_cleanup(): call to MboxCondSend returned %d\n", rc);
			}
			//
			// zap the pager daemon
			//
			zap(ProcTable[i].pid);
		}
	}

	//
	// MMU_Done in order to
	// stop performig virtual memory
	//
	MMU_Done();

	//
	// free up the swap disk
	//
	free_disk(SWAP_DISK);

	//
	// set the free frame and free track counts
	//
	vmStats.freeFrames = free_frame_count();
	vmStats.freeBlocks = disk_blocks(SWAP_DISK);


	//
	// Print vm statistics.
	//
	PrintStats();

} /* vm_cleanup */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void
FaultHandler(int type /* MMU_INT */,
             int offset  /* Offset within VM region */)
{
	//
	// kernel level process
	//
	int page_index = offset / MMU_PageSize();

	int cause;
	int pid;
	int rc;
	int proc_slot;

	assert(type == MMU_INT);
	cause = MMU_GetCause();
	assert(cause == MMU_FAULT);
	vmStats.faults++;

	//
	// send my process id to the PageFault queue
	//
	pid=getpid();
	proc_slot=find_proc(pid);
	faults[proc_slot].page_faulted = page_index;
	if (ProcTable[proc_slot].pageTable[page_index].state==EMPTY){
		//
		// keep track of previously unused page count
		//
		vmStats.new++;
	}
	rc=MboxSend(fault_mbox, (void*)&pid, sizeof(int));
	if (rc < 0){
		fprintf(stderr, "FaultHandler(): call to MboxSend returned %d\n", rc);
	}

	//
	// block myself, until the page fault has been
	// taken care of by one of the pager daemons
	//
	rc=MboxReceive(ProcTable[proc_slot].p_mbox, NULL, 0);
	if (rc < 0){
		fprintf(stderr, "FaultHandler(): call to MboxReceive returned %d\n", rc);
		//terminate_real(-1);
	}

	if (disk_blocks(SWAP_DISK)-free_block_count(SWAP_DISK)==disk_blocks(SWAP_DISK)){
		terminate_real(DISK_FULL);
	}
/*
	if (ProcTable[proc_slot].status==DISK_FULL){
		terminate_real(DISK_FULL);
	}
*/
} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
	//
	// kernel level process
	//

	//
	// add myself to the process table
	//
	int pid=getpid();
	int daemon_slot=addto_proctable(pid);
	ProcTable[daemon_slot].status=DAEMON;

	int fpid;
	int new_proc_slot;
	int old_proc_slot;
	int free_frame;
	int new_page;
	int old_page;
	int track;
	int rc=0;
	int access;

	while(! is_zapped()) {
		/* Wait for fault to occur (receive from mailbox) */
		//
		// the message received is the process id of the offending
		// process, look this up in the fault table
		//
		rc=MboxReceive(fault_mbox, (void*)&fpid, sizeof(int));


		if (rc < 0 || PagerZapFlag){
			if (is_zapped() || PagerZapFlag){ // Pager was zapped or are being zapped.
				if (PagerZapFlag){ // if any other pager was zapped but you are running.
					MboxCondSend(fault_mbox, NULL, 0);
				}
				return 0;

			} else {
				fprintf(stderr, "Pager(): call to MboxReceive returned %d\n", rc);
			}
		}

		//
		// get the page that caused the page fault
		//
		new_proc_slot=find_proc(fpid);
		new_page = faults[new_proc_slot].page_faulted;

		//
		// see if the page is already mapped to a frame
		//
		rc = find_page(new_page, fpid);
		if (rc < 0){
			//
			// Look for free frame
			//
			free_frame = find_empty_frame();
			if (free_frame < 0){
				//
				// if there is not a free frame, then one must be replaced,
				// keep track of this in vmStats
				//
				vmStats.replaced++;

				//
				// If there isn't one then use clock algorithm to
				// replace a page (perhaps write to disk)
				//
				free_frame = find_dump_frame();

				//
				// get the process slot of the process who's frame
				// is going to be dumped to disk
				//
				old_proc_slot = find_proc(frameTable[free_frame].owner_pid);

				//
				// see if the frame is dirty
				//
				access=0;
				rc=MMU_GetAccess(free_frame, &access);
				if (rc < 0){
					fprintf(stderr, "Pager(): call to MMU_GetAccess returned %d\n", rc);
				}
				if (access & MMU_DIRTY){
					//
					// save the dirty frame to disk
					//
					old_page = frameTable[free_frame].page_offset;
					track = ProcTable[old_proc_slot].pageTable[old_page].track;
					//
					// temp. map page to frame to get access to the
					// frame we are about to save to disk
					//
					rc=MMU_Map(TAG, old_page, free_frame, MMU_PROT_RW);
					if (rc < 0){
						fprintf(stderr, "Pager(): call to MMU_Map returned %d\n", rc);
					}
					//
					// Copy Frame's contents into a temporary buf
					//
					memcpy(ProcTable[old_proc_slot].pagerBuf, (VMR+(old_page*PAGE_SIZE)), PAGE_SIZE);
					rc=MMU_Unmap(TAG, old_page);
					if (rc < 0){
						fprintf(stderr, "Pager(): call to MMU_Unmap returned %d\n", rc);
					}

					if (track==NONE){
						//
						// take a dump on any track which is free
						//
						rc = disk_dump(SWAP_DISK, DUMP_ANY, ProcTable[old_proc_slot].pagerBuf);
						if (rc < 0){
							if (rc == DISK_FULL){
								ProcTable[new_proc_slot].status=DISK_FULL;
							} else {
								fprintf(stderr, "Pager(): call to disk_dump returned %d\n", rc);
							}
						} else {
							//
							// save track we dumped on
							//
							ProcTable[old_proc_slot].pageTable[old_page].track = rc;
						}
					} else {
						//
						// dump to the same track the old info was stored on
						//
						rc = disk_dump(SWAP_DISK, track, ProcTable[old_proc_slot].pagerBuf);
						if (rc < 0){
							if (rc == DISK_FULL){
								ProcTable[new_proc_slot].status=DISK_FULL;
							} else {
								fprintf(stderr, "Pager(): call to disk_dump returned %d\n", rc);
							}
						}
					}

					//
					// increment pageOuts
					//
					vmStats.pageOuts++;

					//
					// remember it is on disk
					//
					ProcTable[old_proc_slot].pageTable[old_page].state = ONDISK;

				} // end if access==MMU_DIRTY

				//
				// set frame back to NONE
				//
				ProcTable[old_proc_slot].pageTable[old_page].frame = NONE;
			}

			//
			// initialize the process clock hand to point to
			// this page if the clock hand has not been init.
			//
			if (clock_hand==NONE){
				clock_hand = free_frame;
			}

			//
			// set frame pid and page which reference it
			// and set its state to INUSE
			//
			frameTable[free_frame].owner_pid = fpid;
			frameTable[free_frame].page_offset = new_page;
			frameTable[free_frame].state = INUSE;

			//
			// set page table state and frame
			//
			if (ProcTable[new_proc_slot].pageTable[new_page].state==EMPTY){
				ProcTable[new_proc_slot].pageTable[new_page].state=FIRST_TIME;
			} else
				if (ProcTable[new_proc_slot].pageTable[new_page].state==ONDISK
				||  ProcTable[new_proc_slot].pageTable[new_page].track != NONE){

				ProcTable[new_proc_slot].pageTable[new_page].state=INUSE;

				//
				// increment page ins
				//
				vmStats.pageIns++;

				//
				// read from disk
				//
				rc = disk_puke(SWAP_DISK,
											 ProcTable[new_proc_slot].pageTable[new_page].track,
											 ProcTable[new_proc_slot].pagerBuf);
				if (rc < 0){
					fprintf(stderr, "disk_puke returned %d\n", rc);
				}

				rc=MMU_Map(TAG, new_page, free_frame, MMU_PROT_RW);
				if (rc < 0){
					fprintf(stderr, "Pager(): call to MMU_Map returned %d\n", rc);
				}
				memcpy(VMR+(new_page*PAGE_SIZE), ProcTable[new_proc_slot].pagerBuf, PAGE_SIZE);
				MMU_Unmap(TAG, new_page);
				if (rc < 0){
					fprintf(stderr, "Pager(): call to MMU_Unmap returned %d\n", rc);
				}
			}

			ProcTable[new_proc_slot].pageTable[new_page].frame=free_frame;

			//
			// init. access to frame to 0, this means that
			// the page is not referenced and is not dirty
			//
			rc=MMU_SetAccess(free_frame, 0);
			if (rc < 0){
				fprintf(stderr, "Pager(): call to MMU_SetAccess returned %d\n", rc);
			}

		} // end if page does not exists on a frame

		//
		// Unblock waiting (faulting) process
		//
		rc=MboxSend(ProcTable[new_proc_slot].p_mbox, NULL, 0);
		if (rc < 0){
			fprintf(stderr, "Pager(): call to MboxSend returned %d\n", rc);
		}

	} // end while
	printf("About to exit Pager \n");
	return 0;
} /* Pager */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the console.
 *
 *----------------------------------------------------------------------
 */
static void PrintStats(void)
{
	console("VmStats\n");
  console("pages:\t\t%d\n", vmStats.pages);
  console("frames:\t\t%d\n", vmStats.frames);
  console("blocks:\t\t%d\n", vmStats.blocks);
  console("freeFrames:\t%d\n", vmStats.freeFrames);
  console("freeBlocks:\t%d\n", vmStats.freeBlocks);
  console("switches:\t%d\n", vmStats.switches);
  console("faults:\t\t%d\n", vmStats.faults);
  console("new:\t\t%d\n", vmStats.new);
  console("pageIns:\t%d\n", vmStats.pageIns);
  console("pageOuts:\t%d\n", vmStats.pageOuts);
  console("replaced:\t%d\n", vmStats.replaced);
} /* PrintStats */

