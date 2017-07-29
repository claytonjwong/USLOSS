/*
 * quit.c
 *	
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>
#include <string.h>

#include "test5.h"

#define PAGES		2
#define FRAMES		2
#define ITERATIONS	1
#define MAPPINGS	PAGES
#define PAGERS		2
#define PRIORITY	5

static int child;
static int parent;

#define CREATE(mboxPtr) verify(SemCreate(0,mboxPtr) == 0)
#define SEND(mbox, msg) verify(SemV(mbox) == 0)
#define RECV(mbox, msg) verify(SemP(mbox) == 0)

void *vmRegion;

#define SYNC(me, other) {	\
    int _dummy;			\
    SEND(other, &_dummy);	\
    RECV(me, &_dummy);		\
}

int
Child(char *arg)
{
    char	*pagePtr;
    int		i;
    int		dummy;

    Tconsole("Child starting\n");
    /*
     * Touch all pages.
     */

    for (i = 0; i < PAGES; i++) {
	Tconsole("Child: %d\n", i);
	pagePtr = PAGE_ADDR(i);
	* ((int *) pagePtr) = i;
    }
    SYNC(child, parent);
    Tconsole("Child continuing\n");
    /*
     * Touch a page, which should cause a page fault and a page-in.
     * Our parent should then terminate us while we're waiting on the
     * fault.
     */
    dummy = * ((int *) PAGE_ADDR(0));
    Tconsole("Child still alive after fault!\n");
    Terminate(117);
    return 0;
} /* Child */

int
Parent(char *arg)
{
    int		pid;
    int		status;
    int		i;
    char	*pagePtr,z;
    char	buf[40];

    Tconsole("Parent starting\n");
    /*
     * Create a child and wait for it to get set up.
     */
    sprintf(buf, "%d", i);
    Spawn("Child", Child, buf, USLOSS_MIN_STACK,3, &pid);
    RECV(parent, &dummy);
    Tconsole("Parent continuing\n");
    /*
     * Flush the child's pages out of memory.
     */
    for (i = 0; i < PAGES; i++) {
	pagePtr = PAGE_ADDR(i);
	z=pagePtr[0];
	/*dummy = * ((int *) pagePtr);
	verify(dummy == 0);*/
    }
    /*
     * Let the child continue, which it should do immediately since it
     * has a higher priority. When we start running again the child
     * should be blocked on a disk I/0.
     */
     SEND(child, &dummy);
     Wait(&pid, &status); 
     Tconsole("Parent terminating\n");
     Terminate(1);
	return 0;
} /* Parent */

int
start5(char *arg)
{
    int		pid;
    int		status;
    int		i;
    char	buf[40];

	Tconsole("start5: Running\n");
	Tconsole("start5: Pagers: %d, Mappings: %d, Pages: %d, Frames: %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, ITERATIONS, PRIORITY);
	vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );
	verify(vmRegion != NULL);

    CREATE(&child);
    CREATE(&parent);
    for (i = 0; i < ITERATIONS; i++) {
        sprintf(buf, "%d", i);
	Spawn("Parent", Parent, buf, USLOSS_MIN_STACK,4, &pid);
	Wait(&pid, &status);
//	PrintStats();
    }
    VmCleanup();
    Terminate(2);
    return 0;
} /* start5 */
