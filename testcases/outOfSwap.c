/*
 * 
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <string.h>
#include <phase5.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "test5.h"

#define TEST		"outOfSwap"
#define PRIORITY	4
#define PAGERS		1

int 	pages;
int 	frames;
int	blocks;
void	*vmRegion;

int
Child(char *arg)
{
	int i;
	char *pagePtr;

    Tconsole("Child(): starting\n\n");

    for (i = 0; i < pages; i++) {
	pagePtr = (char *)PAGE_ADDR(i);
	strcpy(pagePtr,"hello");
	
    }
    Tconsole("Child(): Should not be here.\n");
    Terminate(117);
    return 0;
} /* Child */

int
start5(char *arg)
{
    int		pid,i;
    struct stat stats;
    int		status;
    char	buf[40];

    Tconsole("start5(): Running %s %d\n", TEST);
    stat("disk1", &stats);
    blocks = stats.st_size / MMU_PageSize();
    pages = blocks+2;
    frames = 1;

    Tconsole("start5(): Pagers: %d, Mappings: %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, pages, pages, frames, 1, 1, PRIORITY);
	vmRegion = VmInit( pages, pages, frames, PAGERS );

    sprintf(buf, "%d", i);
    Spawn("Child1", Child, buf, USLOSS_MIN_STACK, PRIORITY, &pid);
    Wait(&pid, &status);
    verify(status != 117);

    Tconsole("start5(): done\n");
    VmCleanup();
    Terminate(1);
	return 0;
} /* start5 */
