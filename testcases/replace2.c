/*
 * replace2.c
 *	
 *	Page replacement.  1 child, 4 pages, 2 frames.  Child
 *      writes pages 0-1. reads the next pages 2-3, reads pages
 *	0-1, and reads 2-3 again. 2 writes (for flushing 0-9 the
 *      first time, and 2 reads (for reading 0-2 back from disk).
 *      occur.
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>
#include <strings.h>

#include "test5.h"

#define TEST		"replace2"
#define PAGES		4
#define CHILDREN 	1
#define FRAMES 		(PAGES / 2)
#define PRIORITY	1
#define ITERATIONS	1
#define MAPPINGS    	PAGES
#define PAGERS      	2

void * vmRegion;

int
Child(char *arg)
{
    char	*string;
    char	buffer[128];
    int		pid;
    int 	page;
    char	c;
    VmStats	before;

    GetPID(&pid);
    Tconsole("Child starting (pid = %d)\n", pid);

    for (page = 0; page < PAGES; page++) {
        sprintf(buffer, "Child %d wrote page %d\n", pid, page);
        string = (char *) (vmRegion + (page * MMU_PageSize()));
        strcpy(string, buffer);
        Tconsole(buffer);
    }
    verify(vmStats.faults == PAGES);
    before = vmStats;

    for (page = FRAMES; page < PAGES; page++) {
        c = *(char *) (vmRegion + (page * MMU_PageSize()));
        sprintf(buffer, "Child %d touched page %d\n", pid, page);
        Tconsole(buffer);
    }

    verify(vmStats.faults - before.faults == 0);
    verify(vmStats.pageOuts - before.pageOuts == 0);

    before = vmStats;

    Tconsole("Child %d again\n", pid);
    for (page = 0; page < FRAMES; page++) {
        sprintf(buffer, "Child %d wrote page %d\n", pid, page);
        Tconsole(buffer);
        string = (char *) (vmRegion + (page * MMU_PageSize()));
        if (strcmp(string, buffer) != 0)
	   Tconsole("Oops!  Read wrong buffer\n");
    }

    verify(vmStats.pageOuts - before.pageOuts == FRAMES);
    verify(vmStats.pageIns - before.pageIns == FRAMES);

    before = vmStats;

    Tconsole("Child %d and again\n", pid);
    for (page = FRAMES; page < PAGES; page++) {
        c = *(char *) (vmRegion + (page * MMU_PageSize()));
        sprintf(buffer, "Child %d touched page %d\n", pid, page);
        Tconsole(buffer);
    }

    verify(vmStats.pageOuts - before.pageOuts == 0);
    verify(vmStats.pageIns - before.pageIns == (PAGES-FRAMES));

    Terminate(117);
    return 0;
} /* Child */

int
start5(char *arg)
{
    int		pid;
    int		status;

    Tconsole("start5(): Running %s\n", TEST);
	 Tconsole("start5(): Pagers: %d, Mappings: %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);
    vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );


    Spawn("Child", Child, NULL, USLOSS_MIN_STACK, PRIORITY, &pid);
    Wait(&pid, &status);
    verify(status == 117);

    Tconsole("start5(): done\n");
    VmCleanup();
    Terminate(1);
    return 0;
} /* start5 */
