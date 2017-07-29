/*
 * simple.c
 *
//One process, Writing and Reading the same data from the page with
//a context switch in between.
//No disk I/O should occur.  0 replaced pages and 1 page
//faults.  Check determanistic statistics.
 */
#include <phase2.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <usloss.h>
#include <string.h>
#include <stdio.h>

#include "test5.h"

#define TEST        "simple"
#define PAGES       1
#define CHILDREN    1
#define FRAMES      1
#define PRIORITY    5
#define ITERATIONS  1
#define PAGERS      1
#define MAPPINGS    PAGES

void *vmRegion;

int sem;

int
Child(char *arg)
{
   int    pid;
   char   str[64]= "This is the first page";

   GetPID(&pid);
   Tconsole("Child(): starting (pid = %d)\n", pid);

   //PrintStats();

   Tconsole("Child(%d): str = %s\n", pid, str);
   Tconsole("Child(%d): strlen(str) = %d\n", pid, strlen(str));

   //memcpy(vmRegion, str, strlen(str));  // does not work(!!)
   //memcpy(vmRegion, str, 23);  // works
   memcpy(vmRegion, str, strlen(str)+1);  // works
/*
   Tconsole("Child(%d): after memcpy\n", pid);

   if (strcmp(vmRegion, str) != 0) {
      Tconsole("Child(): Wrong string read, first attempt\n");
   }

   verify(vmStats.faults == 1);
   verify(vmStats.new == 1);

   SemV(sem);

   if (strcmp(vmRegion, str) != 0) {
      Tconsole("Child(): Wrong string read, second attempt\n");
      abort();
   }

   verify(vmStats.faults == 1);
   verify(vmStats.new == 1);
   verify(vmStats.pageOuts == 0);
   verify(vmStats.pageIns == 0);

   Tconsole("Child(%d): terminating\n", pid);

   Terminate(117);
   return 0;
   */

   return 0;
} /* Child */


int
start5(char *arg)
{
	printf("in start5()\n");

 	 int  pid;
   int  status;

   Tconsole("start5(): Running %s\n", TEST);
   Tconsole("start5(): Pagers: %d, Mappings : %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);

   vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );
   Tconsole("start5(): after call to VmInit\n");
   verify(vmRegion != NULL);
	printf("vmRegion = %d\n", (int)vmRegion);

   SemCreate(0, &sem);

   Spawn("Child", Child, 0, USLOSS_MIN_STACK*7, PRIORITY, &pid);
   SemP( sem);
   Wait(&pid, &status);
   verify(status == 117);

   Tconsole("start5(): done\n");
   //PrintStats();
   VmCleanup();
   Terminate(1);

   return 0;
} /* start5 */
