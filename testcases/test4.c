/*
 * test.c
 *	
 *	Tester for phase5. You can define the number of pages, frames,
 *	children, children priority, and iterations through loop.
 *	The children execute one-at-a-time, as determined by the
 *	controller process. Each child alternately writes a string to 
 *	each page of the VM region, then reads the pages back to make
 *	sure they have the correct contents. I've defined four standard
 *	tests below; you can compile each test by defining TEST1, TEST2,
 *	TEST3, or TEST4 when compiling this file. You'll probably want
 *	to start out with some simpler settings, such as 1 page, 1 frame,
 *	and 1 child.
 *
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>
#include <stdarg.h>
#include <strings.h>

#include "test5.h"

#define TEST4

/*
 * Test 1 -- no page replacement
 */
#if defined(TEST1)
#define TEST		"Test 1"
#define PAGES		4
#define CHILDREN 	4
#define FRAMES 		(PAGES * CHILDREN)
#define PRIORITY	3
#define ITERATIONS	2
#define PAGERS		2

/*
 * Test 2 -- lots of page replacement
 */
#elif defined(TEST2)
#define TEST		"Test 2"
#define PAGES		4
#define CHILDREN 	4
#define FRAMES 		PAGES
#define PRIORITY	3
#define ITERATIONS	1
#define PAGERS		2

/*
 * Test 3 -- minimal situation (1 page, 1 frame, two children, 1 pager)
 */

#elif defined(TEST3)
#define TEST		"Test 3"
#define PAGES		1
#define CHILDREN 	2
#define FRAMES 		PAGES
#define PRIORITY	3
#define ITERATIONS	2
#define PAGERS		1

/*
 * Test 4 -- run children randomly
 */
#elif defined(TEST4)
#define TEST		"Test 4"
#define PAGES		4
#define CHILDREN 	2
#define FRAMES 		((PAGES * CHILDREN) / 2)
#define PRIORITY	3
#define ITERATIONS	4
#define PAGERS		2
#define RANDOM
#endif

/* 
 * Information about each child.
 */
#define ALIVE 	1
#define DONE 	2

typedef struct Child {
    int		id;		/* Child's id */
    int		child;		/* Child waits for parent */
    int		parent;		/* Parent waits for child */
    int		status;		/* Child's status (see above) */
} Child;

Child	*children[CHILDREN];
Child	childInfo[CHILDREN];
int	alive = 0;
int	running;
int	currentChild;
void	*vmRegion;
int	pageSize;

char	*fmt = "** Child %d, page %d";


#define Rand(limit) ((int) (((double)((limit)+1)) * rand() / \
			((double) RAND_MAX)))
#ifdef DEBUG
int debugging = 1;
#else
int debugging = 0;
#endif /* DEBUG */

void
Vm_Config(int *entries, int *pages, int *frames)
{
    *entries = PAGES;
    *pages = PAGES;
    *frames = FRAMES;
} /* Vm_Config */

static void
debug(char *fmt, ...)
{
    va_list ap;

    if (debugging) {
	va_start(ap, fmt);
	vconsole(fmt, ap);
    }
} /* debug */


int
ChildProc(char *arg)
{
    Child	*childPtr;
    int		page;
    int		i;
    char	*target;
    char	buffer[128];
    int		pid;
    int		result;

    GetPID(&pid);
    childPtr = &(childInfo[atoi(arg)]);
    Tconsole("Child %d starting (pid = %d)\n", childPtr->id, pid);
    childPtr->status = ALIVE;
    alive++;
    result = SemV(running);
    assert(result == 0);
    for (i = 0; i < ITERATIONS; i++) {
	debug("Child %d waiting\n", childPtr->id);
	result = SemP(childPtr->child);
	assert(result == 0);
	Tconsole("Child %d writing\n", childPtr->id);
	for (page = 0; page < PAGES; page++) {
	    sprintf(buffer, fmt, childPtr->id, page);
	    target = PAGE_ADDR(page);
	    debug("Child %d writing page %d (0x%p): \"%s\"\n", 
		childPtr->id, page, target, buffer);
	    strcpy(target, buffer);
	}
	debug("Child %d resuming parent\n", childPtr->id);
	result = SemV(childPtr->parent);
	assert(result == 0);
	debug("Child %d waiting (2)\n", childPtr->id);
	result = SemP(childPtr->child);
	assert(result == 0);
	Tconsole("Child %d reading\n", childPtr->id);
	for (page = 0; page < PAGES; page++) {
	    sprintf(buffer, fmt, childPtr->id, page);
	    target = PAGE_ADDR(page);
	    debug("Child %d reading page %d (0x%p)\n", childPtr->id, page,
		target);
	    if (strcmp(target, buffer)) {
		int frame;
		int protection;
		Tconsole(
		    "Child %d: read \"%s\" from page %d (0x%p), not \"%s\"\n",
		    childPtr->id, target, page, target, buffer);
		Tconsole("Child %d: MMU contents:\n", childPtr->id);
		for (i = 0; i < PAGES; i++) {
		    result = MMU_GetMap(0, i, &frame, &protection);
		    if (result == MMU_OK) {
			Tconsole("page %d frame %d protection 0x%x\n",
			    i, frame, protection);
		    }
		}
		abort();
	    }
	}
	if (i == (ITERATIONS - 1)) {
	    childPtr->status = DONE;
	}
	debug("Child %d resuming parent\n", childPtr->id);
	result = SemV(childPtr->parent);
	assert(result == 0);
    }
    Tconsole("Child %d done\n", childPtr->id);
    Terminate(117);
    return 0; // not reached
} /* ChildProc */

int
Controller(char *arg)
{
    static int	index = 0;
    int		result;
    int		pid;

    GetPID(&pid);
    Tconsole("Controller starting, pid = %d\n", pid);
    while(alive > 0) {
	/*
	 * Choose a child to do something
	 */
#ifdef RANDOM
	index = Rand(alive-1);
#else
	index = (index + 1) % alive;
#endif
	debug("Pinging child %d (%d)\n", 
	    children[index]->id, children[index]->child);
	result = SemV(children[index]->child);
	assert(result == 0);
	debug("Waiting for child %d (%d)\n", 
	    children[index]->id, children[index]->child);
	result = SemP(children[index]->parent);
	assert(result == 0);
	if (children[index]->status == DONE) {
	    /*
	     * When a child is done replace it with the alive child 
	     * with the highest index, so that the alive children 
	     * are always in positions 0..alive-1 in the array.
	     */
	    alive--;
	    children[index] = children[alive];
	}
    }
    Tconsole("Controller quitting\n");
    Terminate(117);
    return 0; // not reached
} /* Controller */


int
start5(char *arg)
{
    int  pid;
    int  i;
    int  result;
    int  status;
    char buf[40];

    Tconsole("start5(): Running %s\n", TEST);
    Tconsole("start5(): Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n",
	PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);
    vmRegion = VmInit(PAGES, PAGES, FRAMES, PAGERS);
    if (vmRegion == NULL) {
	Tconsole("start5(): VmInit failed\n");
	Terminate(1);
    }
    result = SemCreate(0, &running);
    assert(result == 0);
    pageSize = MMU_PageSize();
    for (i = 0; i < CHILDREN; i++) {
	children[i] = &childInfo[i];
	children[i]->id = i;
	result = SemCreate(0, &children[i]->child);
	debug("Created child sem %d for child %d\n", 
	    children[i]->child, i);
	assert(result == 0);
	result = SemCreate(0, &children[i]->parent);
	debug("Created parent sem %d for child %d\n", 
	    children[i]->parent, i);
	assert(result == 0);
        sprintf(buf, "%d", i);
	Spawn("ChildProc", ChildProc, buf, USLOSS_MIN_STACK * 2, 
	    PRIORITY, &pid);
	result = SemP(running);
	assert(result == 0);
    }
    Spawn("Controller", Controller, NULL, USLOSS_MIN_STACK * 2, 1, &pid);
    for (i = 0; i < CHILDREN+1; i++) {
	Wait(&pid, &status);
	verify(status == 117);
    }
    Tconsole("start5(): done\n");
    VmCleanup();
    Terminate(1);
    return 0; // not reached
} /* start5 */
