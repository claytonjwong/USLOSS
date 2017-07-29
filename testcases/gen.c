/*
 * gen.c
 *	
 *	Build test.  1 child, 1 page, 1 frame.  Fault for the page
 *      and terminate.
 */
#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>
#include <strings.h>

#include "test5.h"

#define TEST		"gen"
#define PAGES		1
#define CHILDREN 	1
#define FRAMES 		PAGES
#define PRIORITY	5
#define ITERATIONS	1
#define MAPPINGS    	PAGES
#define PAGERS      	1

void * vmRegion;

int
Child(char *arg)
{
    char	*string;
    char	buffer[128];
    int		pid;

    Tconsole("Child\n");
    GetPID(&pid);
    Tconsole("Child starting (pid = %d)\n", pid);
    sprintf(buffer, "Ok!  Child touched vmRegion. \n");
    string = (char *) vmRegion;
    strcpy(string, buffer);
    Tconsole(string);
    Terminate(117);
	return 0;
} /* Child */

int
start5(char *arg)
{
    int		status;
    int		pid;

    Tconsole("start5: Running %s\n", TEST);
	Tconsole("Pagers: %d, Mappings: %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);
	vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );

    Spawn("Child", Child, NULL, USLOSS_MIN_STACK * 2, PRIORITY, &pid);
    Wait(&pid, &status);
    verify(status == 117);

    Tconsole("start5 done");
    Tconsole("\n");
    VmCleanup();
    Terminate(1);
	return 0;
} /* start5 */
