/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

#define DAEMON_PRIORITY 2

#define SWAP_DISK 1

/*
 * Different states for a page.
 */


/* You'll probably want more states */


#define PAGE_SIZE DISK_SECTOR_SIZE * DISK_TRACK_SIZE

/*
 * Page table entry.
 */

typedef struct PTE {
    int  state;  /* See above. */
    int  frame;  /* Frame that stores the page (if any). */
    int  track;  /* Disk block that stores the page (if any). */
    /* Add more stuff here */
} PTE;

/*
 * Frame table entry.
 */
typedef struct FTE {
	int  owner_pid;
	//int  dump;
	int  page_offset;
  int  state;
} FTE;

/*
 * Per-process information.
 */
typedef struct Process {
	int pid;
	int status;
  int  numPages;   /* Size of the page table. */
  PTE  *pageTable; /* The page table for the process. */
  int p_mbox;
  void * pagerBuf;
  /* Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int page_faulted;      /* Address that caused the fault. */
} FaultMsg;

#define CheckMode() assert(psr_get() & PSR_CURRENT_MODE)


enum { ZAPPED=-3, NONE=-1, EMPTY, INUSE, DAEMON, FIRST_TIME, ONDISK };
enum { FALSE=0, TRUE=1 };
//
//
// VmStats, defined in phase5.h:
//
//
//typedef struct VmStats {
//    int pages;          /* Size of VM region, in pages */
//    int frames;         /* Size of physical memory, in frames */
//    int blocks;         /* Size of disk, in blocks (pages) */
//    int freeFrames;     /* # of frames that are not in-use */
//    int freeBlocks;     /* # of blocks that are not in-use */
//    int switches;       /* # of context switches */
//    int faults;         /* # of page faults */
//    int new;            /* # faults caused by previously unused pages*/
//    int pageIns;        /* # faults that required reading page from disk */
//    int pageOuts;       /* # faults that required writing a page to disk */
//    int replaced;       /* # pages replaced */
//} VmStats;


