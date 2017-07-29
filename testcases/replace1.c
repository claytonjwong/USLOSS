/*
 * replace1.c
 *	
//Page replacement.  1 child, 20 pages, 10 frames.  Child
//touches first ten pages then touches the next ten pages.
//No disk I/O should occur.  10 replaced pages and 20 page 
//faults.  Check deterministic statistics.
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <mmu.h>
#include <usloss.h>
#include <string.h>
#include <stdlib.h>
#include <phase5.h>

#include "test5.h"

#define TEST		"replace1"
#define PAGES		20
#define CHILDREN 	1
#define FRAMES 		(PAGES / 2)
#define PRIORITY	5
#define ITERATIONS	1
#define MAPPINGS    	PAGES
#define PAGERS      	2

void *vmRegion;

int
Child(char *arg)
{
    int		pid;
    int 	page;
    char	c;

    GetPID(&pid);
    Tconsole("Child starting (pid = %d)\n", pid);
    Tconsole("\n");

    for (page = 0; page < PAGES / 2; page++) {
        c = *(char *) (vmRegion + (page * (int)MMU_PageSize()));
    }
    verify(vmStats.faults == 10);
    for (page = PAGES / 2; page < PAGES; page++) {
        c = *(char *) (vmRegion + (page * (int)MMU_PageSize()));
    }
    verify(vmStats.faults == 20);
    verify(vmStats.pageIns == 0);
    verify(vmStats.pageOuts == 0);
    Terminate(117);
    return 117;
} /* Child */

int 
start5(char *arg)
{
    int		pid;
    int		status;


    Tconsole("start5(): Running %s\n", TEST);

	console("start5(): Pagers: %d, Mappings: %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);
    vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );

    Spawn("Child", Child, 0, USLOSS_MIN_STACK*7, PRIORITY, &pid);
    Wait(&pid, &status);
    verify(status == 117);

    Tconsole("start5(): done\n");
    //PrintStats();
    VmCleanup();
    Terminate(1);
    return 0;

} /* start5 */
