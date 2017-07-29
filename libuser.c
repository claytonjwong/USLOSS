/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <phase1.h>
#include <phase2.h>
#include <libuser.h>
#include <usyscall.h>
#include <usloss.h>


static void checkmode(char *func_name)
{
   if ( psr_get() & PSR_CURRENT_MODE ) {
      console("Trying to invoke %s syscall from kernel\n", func_name);
      halt(1);
   }
} /* checkmode */


/*
 *  Routine:  Spawn
 *
 *  Description: This is the call entry to fork a new user process.
 *
 *  Arguments:    char *name    -- new process's name
 *		  PFV func      -- pointer to the function to fork
 *		  char *arg	-- argument to function
 *                int stacksize -- amount of stack to be allocated
 *                int priority  -- priority of forked process
 *                int  *pid      -- pointer to output value
 *                (output value: process id of the forked process)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Spawn(char *name, int (*func)(char *), char *arg, int stack_size,
	int priority, int *pid)
{
    sysargs sa;

    checkmode("Spawn");
    sa.number = SYS_SPAWN;
    sa.arg1 = (void *) func;
    sa.arg2 = (void *) arg;
    sa.arg3 = (void *) stack_size;
    sa.arg4 = (void *) priority;
    sa.arg5 = (void *) name;
    usyscall(&sa);
    *pid = (int) sa.arg1;
    return (int) sa.arg4;
} /* end of Spawn */


/*
 *  Routine:  Wait
 *
 *  Description: This is the call entry to wait for a child completion
 *
 *  Arguments:    int *pid -- pointer to output value 1
 *                (output value 1: process id of the completing child)
 *                int *status -- pointer to output value 2
 *                (output value 2: status of the completing child)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Wait(int *pid, int *status)
{
    sysargs sa;

    checkmode("Wait");
    sa.number = SYS_WAIT;
    usyscall(&sa);
    *pid = (int) sa.arg1;
    *status = (int) sa.arg2;
    return (int) sa.arg4;

} /* End of Wait */


/*
 *  Routine:  Terminate
 *
 *  Description: This is the call entry to terminate
 *               the invoking process and its children
 *
 *  Arguments:   int status -- the commpletion status of the process
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
void Terminate(int status)
{
    sysargs sa;

    checkmode("Terminate");
    sa.number = SYS_TERMINATE;
    sa.arg1 = (void *) status;
    usyscall(&sa);
    return;

} /* End of Terminate */


/*
 *  Routine:  SemCreate
 *
 *  Description: Create a semaphore.
 *
 *
 *  Arguments:    int value -- initial semaphore value
 *		  int *semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemCreate(int value, int *semaphore)
{
    sysargs sa;

    checkmode("SemCreate");
    sa.number = SYS_SEMCREATE;
    sa.arg1 = (void *) value;
    usyscall(&sa);
    *semaphore = (int) sa.arg1;
    return (int) sa.arg4;
} /* end of SemCreate */


/*
 *  Routine:  SemP
 *
 *  Description: "P" a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemP(int semaphore)
{
    sysargs sa;

    checkmode("SemP");
    sa.number = SYS_SEMP;
    sa.arg1 = (void *) semaphore;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of SemP */


/*
 *  Routine:  SemV
 *
 *  Description: "V" a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemV(int semaphore)
{
    sysargs sa;

    checkmode("SemV");
    sa.number = SYS_SEMV;
    sa.arg1 = (void *) semaphore;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of SemV */


/*
 *  Routine:  SemFree
 *
 *  Description: Free a semaphore.
 *
 *
 *  Arguments:    int semaphore -- semaphore handle
 *                (output value: completion status)
 *
 */
int SemFree(int semaphore)
{
    sysargs sa;

    checkmode("SemFree");
    sa.number = SYS_SEMFREE;
    sa.arg1 = (void *) semaphore;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of SemFree */


/*
 *  Routine:  GetTimeofDay
 *
 *  Description: This is the call entry point for getting the time of day.
 *
 *  Arguments:    int *tod  -- pointer to output value
 *                (output value: the time of day)
 *
 */
void GetTimeofDay(int *tod)
{
    sysargs sa;

    checkmode("GetTimeofDay");
    sa.number = SYS_GETTIMEOFDAY;
    usyscall(&sa);
    *tod = (int) sa.arg1;
    return;
} /* end of GetTimeofDay */


/*
 *  Routine:  CPUTime
 *
 *  Description: This is the call entry point for the process' CPU time.
 *
 *
 *  Arguments:    int *cpu  -- pointer to output value
 *                (output value: the CPU time of the process)
 *
 */
void CPUTime(int *cpu)
{
    sysargs sa;

    checkmode("CPUTime");
    sa.number = SYS_CPUTIME;
    usyscall(&sa);
    *cpu = (int) sa.arg1;
    return;
} /* end of CPUTime */


/*
 *  Routine:  GetPID
 *
 *  Description: This is the call entry point for the process' PID.
 *
 *
 *  Arguments:    int *pid  -- pointer to output value
 *                (output value: the PID)
 *
 */
void GetPID(int *pid)
{
    sysargs sa;

    checkmode("GetPID");
    sa.number = SYS_GETPID;
    usyscall(&sa);
    *pid = (int) sa.arg1;
    return;
} /* end of GetPID */


/*
 *  Routine:  Sleep
 *
 *  Description: This is the call entry point for timed delay.
 *
 *  Arguments:    int seconds -- number of seconds to sleep
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Sleep(int seconds)
{
    sysargs sa;

    checkmode("Sleep");
    sa.number = SYS_SLEEP;
    sa.arg1 = (void *) seconds;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of Sleep */


/*
 *  Routine:  DiskRead
 *
 *  Description: This is the call entry point for disk input.
 *
 *  Arguments:    void* dbuff  -- pointer to the input buffer
 *                int   unit -- which disk to read
 *                int   track  -- first track to read
 *                int   first -- first sector to read
 *                int   sectors -- number of sectors to read
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskRead(void *dbuff, int unit, int track, int first, int sectors,
    int *status)
{
    sysargs sa;

    checkmode("DiskRead");
    sa.number = SYS_DISKREAD;
    sa.arg1 = dbuff;
    sa.arg2 = (void *) sectors;
    sa.arg3 = (void *) track;
    sa.arg4 = (void *) first;
    sa.arg5 = (void *) unit;
    usyscall(&sa);
    *status = (int) sa.arg1;
    return (int) sa.arg4;
} /* end of DiskRead */


/*
 *  Routine:  DiskWrite
 *
 *  Description: This is the call entry point for disk output.
 *
 *  Arguments:    void* dbuff  -- pointer to the output buffer
 *		  int   unit -- which disk to write
 *                int   track  -- first track to write
 *                int   first -- first sector to write
 *		  int	sectors -- number of sectors to write
 *                int   *status    -- pointer to output value
 *                (output value: completion status)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskWrite(void *dbuff, int unit, int track, int first, int sectors,
    int *status)
{
    sysargs sa;

    checkmode("DiskWrite");
    sa.number = SYS_DISKWRITE;
    sa.arg1 = dbuff;
    sa.arg2 = (void *) sectors;
    sa.arg3 = (void *) track;
    sa.arg4 = (void *) first;
    sa.arg5 = (void *) unit;
    usyscall(&sa);
    *status = (int) sa.arg1;
    return (int) sa.arg4;
} /* end of DiskWrite */


/*
 *  Routine:  DiskSize
 *
 *  Description: This is the call entry point for getting the disk size.
 *
 *  Arguments:    int	unit -- which disk
 *		  int	*sector -- # bytes in a sector
 *		  int	*track -- # sectors in a track
 *		  int   *disk -- # tracks in the disk
 *                (output value: completion status)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int DiskSize(int unit, int *sector, int *track, int *disk)
{
    sysargs sa;

    checkmode("DiskSize");
    sa.number = SYS_DISKSIZE;
    sa.arg1 = (void *) unit;
    usyscall(&sa);
    *sector = (int) sa.arg1;
    *track = (int) sa.arg2;
    *disk = (int) sa.arg3;
    return (int) sa.arg4;
} /* end of DiskSize */


/*
 *  Routine:  TermRead
 *
 *  Description: This is the call entry point for terminal input.
 *
 *  Arguments:    char *buff    -- pointer to the input buffer
 *                int   bsize   -- maximum size of the buffer
 *                int   unit_id -- terminal unit number
 *                int  *nread      -- pointer to output value
 *                (output value: number of characters actually read)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int TermRead(char *buff, int bsize, int unit_id, int *nread)
{
    sysargs sa;

    checkmode("TermRead");
    sa.number = SYS_TERMREAD;
    sa.arg1 = (void *) buff;
    sa.arg2 = (void *) bsize;
    sa.arg3 = (void *) unit_id;
    usyscall(&sa);
    *nread = (int) sa.arg2;
    return (int) sa.arg4;
} /* end of TermRead */


/*
 *  Routine:  TermWrite
 *
 *  Description: This is the call entry point for terminal output.
 *
 *  Arguments:    char *buff    -- pointer to the output buffer
 *                int   bsize   -- number of characters to write
 *                int   unit_id -- terminal unit number
 *                int  *nwrite      -- pointer to output value
 *                (output value: number of characters actually written)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite)
{
    sysargs sa;

    checkmode("TermWrite");
    sa.number = SYS_TERMWRITE;
    sa.arg1 = (void *) buff;
    sa.arg2 = (void *) bsize;
    sa.arg3 = (void *) unit_id;
    usyscall(&sa);
    *nwrite = (int) sa.arg2;
    return (int) sa.arg4;
} /* end of TermWrite */


/*
 *  Routine:  Mbox_Create
 *
 *  Description: This is the call entry point to create a new mail box.
 *
 *  Arguments:    int   numslots -- number of mailbox slots
 *                int   slotsize -- size of the mailbox buffer
 *                int  *mid      -- pointer to output value
 *                (output value: id of created mailbox)
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Create(int numslots, int slotsize, int *mid)
{
    sysargs sa;

    checkmode("Mbox_Create");
    sa.number = SYS_MBOXCREATE;
    sa.arg1 = (void *) numslots;
    sa.arg2 = (void *) slotsize;
    usyscall(&sa);
    *mid = (int) sa.arg1;
    return (int) sa.arg4;
} /* end of Mbox_Create */


/*
 *  Routine:  Mbox_Release
 *
 *  Description: This is the call entry point to release a mailbox
 *
 *  Arguments: int mbox  -- id of the mailbox
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Release(int mbox)
{
    sysargs sa;

    checkmode("Mbox_Release");
    sa.number = SYS_MBOXRELEASE;
    sa.arg1 = (void *) mbox;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of Mbox_Release */


/*
 *  Routine:  Mbox_Send
 *
 *  Description: This is the call entry point mailbox send.
 *
 *  Arguments:    int mbox -- id of the mailbox to send to
 *                int size -- size of the message
 *                void* msg  -- message to send
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Send(int mbox, int size, void *msg)
{
    sysargs sa;

    checkmode("Mbox_Send");
    sa.number = SYS_MBOXSEND;
    sa.arg1 = (void *) mbox;
    sa.arg2 = (void *) msg;
    sa.arg3 = (void *) size;
    usyscall(&sa);
    return (int) sa.arg4;
} /* end of Mbox_Send */


/*
 *  Routine:  Mbox_Receive
 *
 *  Description: This is the call entry point for terminal input.
 *
 *  Arguments:    int mbox -- id of the mailbox to receive from
 *                int size -- size of the buffer
 *                void* msg  -- location to receive message
 *
 *  Return Value: 0 means success, -1 means error occurs
 *
 */
int Mbox_Receive(int mbox, int size, void *msg)
{
    sysargs sa;

    checkmode("Mbox_Receive");
    sa.number = SYS_MBOXRECEIVE;
    sa.arg1 = (void *) mbox;
    sa.arg2 = (void *) msg;
    sa.arg3 = (void *) size;
    usyscall( &sa );
        /*
         * This doesn't belong here. The copy should by done by the
         * system call.
         */
        if ( (int) sa.arg4 == -1 )
                return (int) sa.arg4;
        // memcpy( (char*)msg, (char*)sa.arg2, (int)sa.arg3);
        return 0;

} /* end of Mbox_Receive */


/*
 *  Routine:  Mbox_CondSend
 *
 *  Description: This is the call entry point mailbox conditional send.
 *
 *  Arguments:    int mbox -- id of the mailbox to send to
 *                int size -- size of the message
 *                char* msg  -- message to send
 *
 *  Return Value: 0 means success, -1 means error occurs, 1 means mailbox
 *                was full
 *
 */
int Mbox_CondSend(int mbox, int size, void *msg)
{
    sysargs sa;

    checkmode("Mbox_CondSend");
    sa.number = SYS_MBOXCONDSEND;
    sa.arg1 = (void *) mbox;
    sa.arg2 = (void *) msg;
    sa.arg3 = (void *) size;
    usyscall(&sa);
    return ((int) sa.arg4);
} /* end of Mbox_CondSend */


/*
 *  Routine:  Mbox_CondReceive
 *
 *  Description: This is the call entry point mailbox conditional
 *               receive.
 *
 *  Arguments:    int mbox -- id of the mailbox to receive from
 *                int size -- size of the buffer
 *                char* msg  -- location to receive message
 *
 *  Return Value: 0 means success, -1 means error occurs, 1 means no
 *                message was available
 *
 */
int Mbox_CondReceive(int mbox, int size, void *msg)
{
    sysargs sa;

    checkmode("Mbox_CondReceive");
    sa.number = SYS_MBOXCONDRECEIVE;
    sa.arg1 = (void *) mbox;
    sa.arg2 = (void *) msg;
    sa.arg3 = (void *) size;
    usyscall( &sa );
    return ((int) sa.arg4);
} /* end of Mbox_CondReceive */


/*
 *  Routine:  VmInit
 *
 *  Description: Initializes the virtual memory system.
 *
 *  Arguments:    int mappings -- # of mappings in the MMU
 *                int pages -- # pages in the VM region
 *                int frames -- # physical page frames
 *                int pagers -- # pagers to use
 *
 *  Return Value: address of VM region, NULL if there was an error
 *
 */
void *VmInit(int mappings, int pages, int frames, int pagers)
{
    sysargs     sa;

    checkmode("VmInit");

    sa.number = SYS_VMINIT;
    sa.arg1 = (void *) mappings;
    sa.arg2 = (void *) pages;
    sa.arg3 = (void *) frames;
    sa.arg4 = (void *) pagers;
    usyscall(&sa);
    if ((int) sa.arg4 == -1) {
        return NULL;
    } else {
        return sa.arg1;
    }
} /* VmInit */


/*
 *  Routine:  VmCleanup
 *
 *  Description: Tears down the VM system
 *
 *  Arguments:
 *
 *  Return Value:
 *
 */

void
VmCleanup(void) {
    sysargs     sa;

    checkmode("VmCleanup");
    sa.number = SYS_VMCLEANUP;
    usyscall(&sa);
} /* VmCleanup */

/* end libuser.c */
