/*
 * simple5.c
 *
 * One process writes every page, where frames = pages-1. If the clock
 * algorithm starts with frame 0, this will cause a page fault on every
 * access. 
 */
#include <phase2.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <usloss.h>
#include <string.h>

#include "test5.h"

#define TEST        "simple5"
#define PAGES       7
#define CHILDREN    1
#define FRAMES      (PAGES-1)
#define PRIORITY    5
#define ITERATIONS  10
#define PAGERS      1
#define MAPPINGS    PAGES

void *vmRegion;

int sem;

int
Child(char *arg)
{
   int      pid;
   int      page;
   int      i;
   char     *buffer;
   VmStats  before;
   int      value;

   GetPID(&pid);
   Tconsole("Child(): starting (pid = %d)\n", pid);
   buffer = (char *) vmRegion;

   for (i = 0; i < ITERATIONS; i++) {
      Tconsole("Child(): iteration %d\n", i);
      before = vmStats;
      for (page = 0; page < PAGES; page++) {
         * ((int *) (vmRegion + (page * MMU_PageSize()))) = page;
         value = * ((int *) (vmRegion + (page * MMU_PageSize())));
         verify(value == page);
      }
      verify(vmStats.faults - before.faults == PAGES);
   }
   verify(vmStats.faults == PAGES * ITERATIONS);
   verify(vmStats.new == PAGES);
   verify(vmStats.pageIns == PAGES * (ITERATIONS - 1));
   verify(vmStats.pageOuts == PAGES * ITERATIONS - FRAMES);

   SemV(sem);

   Terminate(117);
   return 0;
} /* Child */


int
start5(char *arg)
{
   int  pid;
   int  status;

   Tconsole("start5(): Running %s\n", TEST);
   Tconsole("start5(): Pagers: %d, Mappings : %d, Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d.\n", PAGERS, MAPPINGS, PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY);

   vmRegion = VmInit( MAPPINGS, PAGES, FRAMES, PAGERS );

   Spawn("Child", Child,  0,USLOSS_MIN_STACK*7,PRIORITY, &pid);
   SemP( sem);
   Wait(&pid, &status);
   verify(status == 117);

   Tconsole("start5(): done\n");
   //PrintStats();
   VmCleanup();
   Terminate(1);

   return 0;
} /* start5 */
