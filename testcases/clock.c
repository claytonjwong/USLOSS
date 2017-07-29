/*
 * clock.c
 *	
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>

#include "test5.h"

#define PAGES	3
#define FRAMES	3
#define PRIORITY    5
#define ITERATIONS  1
#define MAPPINGS    PAGES
#define PAGERS      2


static int child1;
static int child2;
int size;

void * vmRegion;

#define CREATE(mboxPtr) verify(Mbox_Create(1, sizeof(int), mboxPtr) == 0)
#define SEND(mbox, msg) verify(Mbox_Send(mbox, sizeof(int), (char *) (msg)) == 0)
#define RECV(mbox, msg) verify(Mbox_Receive(mbox, sizeof(int), (char *) (msg)) == 0)

#define SYNC(me, other) {	\
    int _dummy;			\
    SEND(other, &_dummy);	\
    RECV(me, &_dummy);		\
}

int
Child1(char *arg)
{
    char	*pagePtr;
    int		i;
    int		dummy;
    int		faults;

    RECV(child1, &dummy);
    /*
     * Touch all pages.
     */

    for (i = 0; i < PAGES; i++) {
	pagePtr = PAGE_ADDR(i);
	dummy = * ((int *) pagePtr);
	verify(dummy == 0);
    }
    SYNC(child1, child2);
    /*
     * Touch the second page -- shouldn't fault.
     */
    faults = vmStats.faults;
    dummy = * ((int *) PAGE_ADDR(1));
    verify((vmStats.faults - faults) == 0);
    SYNC(child1, child2);
    /*
     * Touch the second page again -- shouldn't fault.
     */
    faults = vmStats.faults;
    dummy = * ((int *) PAGE_ADDR(1));
    verify((vmStats.faults - faults) == 0);
    Terminate(117);
    return 0;  // not reached
} /* Child1 */

int 
Child2(char *arg) 
{
    VmStats	old;
    int		dummy;

    SYNC(child2, child1);
    /*
     * Touch a page -- should cause a fault and replacement.
     */
    old = vmStats;
    dummy = * ((int *) PAGE_ADDR(0));
    verify((vmStats.faults - old.faults) == 1);
    verify((vmStats.replaced - old.replaced) == 1);
    SYNC(child2, child1);
    /*
     * Touch another page -- should cause a fault and replacement.
     */
    old = vmStats;
    dummy = * ((int *) PAGE_ADDR(1));
    verify((vmStats.faults - old.faults) == 1);
    verify((vmStats.replaced - old.replaced) == 1);
    SEND(child1, &dummy);
    Terminate(117);
    return 0; // not reached
} /* Child2 */

int
start5(char *arg)
{
    int		pid;
    int		child;
    int		status;

    Tconsole("start5: Running\n");
    vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );
    CREATE(&child1);
    CREATE(&child2);
    Spawn("Child1", Child1, NULL, USLOSS_MIN_STACK, PRIORITY, &pid);
    Spawn("Child2", Child2, NULL, USLOSS_MIN_STACK, PRIORITY, &pid);
    Wait(&pid, &status);
    verify(status == 117);
    Wait(&pid, &status);
    verify(status == 117);
    verify(vmStats.faults == 5);
    verify(vmStats.replaced == 2);
    VmCleanup();
    Terminate(1);
} /* start5 */
