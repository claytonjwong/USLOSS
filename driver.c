//////////////////////////////////////////////////////
//
// Programmers: Boris Salazar and Clayton Wong
//
// driver.c
//
// description: device drivers for clock, disks,
// and terminal units
//
//////////////////////////////////////////////////////

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <strings.h>
#include <driver.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>
#include <queue.h>
#include "provided_prototypes.h"


/* ------------------------------------- testcases --------------------------------------------

test00 - OK
test01 - OK
test02 - OK
test03 - OK
test04 - OK
test05 - OK
test06 - OK
test07 - OK
test08 - OK
test09 - OK
test10 - OK
test11 - OK
test12 - OK
test13 - OK
test14 - OK
test15 - OK
test16 - OK
test17 - OK
test18 - OK
test19 - OK
test20 - OK
test21 - OK
test22 - OK
*/


/* ------------------------------------- globals --------------------------------------------*/

//
// semaphores
//
int running;        // allows driver processes
                    // to start running
int disk1req_sem;   // service request for disk1
int disk2req_sem;   // service request for disk2
int	disk1done_sem;  // done message for disk1
int	disk2done_sem;  // done message for disk2

//
// disks
//
int disk1ntrack;    // number of tracks
                    // on disk1
int disk2ntrack;    // number of tracks
                    // on disk2
//
// disk operations
//
#define READ 'r'
#define WRITE 'w'

diskreq_ptr disk1q=NULL;   // queue for disk1 requests
diskreq_ptr disk2q=NULL;   // queue for disk2 requests

//
// current track head of disk is on
//
int disk_head[DISK_UNITS];

//
// terminals
//
int termread_mbox[TERM_UNITS];       // used for synchronization                                     
int termreadbuf_mbox[TERM_UNITS];    // buffer up to 10 lines of data
int termwrite_mbox[TERM_UNITS];      // used for synchronization
int termwritebuf_mbox[TERM_UNITS];   // used to send buffered message
int termwritecomp_mbox[TERM_UNITS];  // used for write done message
int term_mutex[TERM_UNITS];          // mutual exclusion for each terminal

//
// terminal writing buffers
//
char* writeline[TERM_UNITS];	// writeline array points to each writeline buffer
char writeline0[MAXLINE];       // writing buffer for terminal 0 
char writeline1[MAXLINE];       // writing buffer for terminal 1
char writeline2[MAXLINE];       // writing buffer for terminal 2
char writeline3[MAXLINE];       // writing buffer for terminal 3
int writelen[TERM_UNITS];       // the len of the buffers
int write_i[TERM_UNITS];        // current index to service
int termread_done[TERM_UNITS];  // keeps track of which terminal
								// have exhausted their read input

int writing[TERM_UNITS];        // flag used to keep track of writing status
int status[TERM_UNITS];         // status used for waitdevice
int control[TERM_UNITS];        // control used for device output

//
// process table
//
struct proc_struct ProcTable4[MAXPROC];   // process table keeps track of
                                          // process information
int ProcTable_mbox;          // process table mutex
void* ZERO_BUF;              // zero sized buffer used for synchronization messages
char line_to_send[MAXLINE];  // intermediate buffer used to send messages containing text

/* ------------------------------------- prototypes --------------------------------------------*/
extern int start4(char *arg);
static int	ClockDriver(char *);
static int	DiskDriver(char *);
static void diskdriver_real(int unit);
static int TermDriver(char *arg_c);
static int termdriver_read(char* arg_c);
static int termdriver_write(char* arg_c);
static void init_proc(int index);
static int find_proc(int pid);
static void to_user_mode(char* func_name);
static void kernel_sleep(sysargs * sa);
static void sleep_real(int sleep_time);
static void kernel_diskread(sysargs* sa);
int disk_read(int unit, int track, int first, int sectors, void* buf);
static void kernel_diskwrite(sysargs* sa);
int disk_write(int unit, int track, int first, int sectors, void* buf);
static void kernel_disksize(sysargs* sa);
int disk_size(int unit, int* sector , int* track, int* disk);
static void kernel_termread(sysargs* sa);
int term_read(int unit, int size, char* buf);
static void kernel_termwrite(sysargs* sa);
int term_write(int unit, int buf_sz,char* buf);
static int addto_proctable(int pid);


//////////////////////////////////////////////////////////////////////
//
// start3
//
// description: starting point for phase4, initializes this bad boy!
//
void
start3(void)
{
    char	name[128];  // used to name processes
    int		i;          // used for iteration
    int		clockPID;   // keeps track of clock process id to zap it later
    int 	disk1PID;   // keeps track of disk1 process id to zap it later
	int		disk2PID;   // keeps track of disk2 process id to zap it later
	int term1PID[3];    // keeps track of term1 process id to zap it later
	int term2PID[3];    // keeps track of term2 process id to zap it later
	int term3PID[3];    // keeps track of term3 process id to zap it later
	int term4PID[3];    // keeps track of term4 process id to zap it later
    int		pid;		// used to zap original termdriver process
    int pid_read;       // used to zap termdriver_read process
    int pid_write;      // used to zap termdriver_write process
    int		status;     // keeps track of the status of start4 when it returns
    char buf[10];       // used to create process names
    int dummy;          // dummy var used to take up space in disk_size call
    int index;          // used for iteration

	//
	// Initialize Proc
	//
	for (index=0; index <MAXPROC; index++){
		init_proc(index);
	}

	//
	// create Proc Table Mutex Mbox
	//
	ProcTable_mbox = MboxCreate(1,0);

    //
    // Check kernel mode here.
    //
	sys_vec[SYS_SLEEP]= kernel_sleep;
	sys_vec[SYS_DISKREAD]= kernel_diskread;
	sys_vec[SYS_DISKWRITE]= kernel_diskwrite;
	sys_vec[SYS_DISKSIZE]= kernel_disksize;
	sys_vec[SYS_TERMWRITE]= kernel_termwrite;
	sys_vec[SYS_TERMREAD]= kernel_termread;

    //
    // Create clock device driver
    // I am assuming a semaphore here for coordination.  A mailbox can
    // be used instead -- your choice.
    //
    running = semcreate_real(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
		console("start3(): Can't create clock driver\n");
		halt(1);
    }
    //
    // Wait for the clock driver to start. The idea is that ClockDriver
    // will V the semaphore "running" once it is running.
    //
    semp_real(running);

    //
    // Create the disk device drivers here.  You may need to increase
    // the stack size depending on the complexity of your
    // driver, and perhaps do something with the pid returned.
    //
    disk1done_sem = semcreate_real(0);
	disk2done_sem = semcreate_real(0);
	disk1req_sem = semcreate_real(0);
	disk2req_sem = semcreate_real(0);
    for (i = 0; i < DISK_UNITS; i++) {
		if (i==0){
			strcpy(name, "Disk 1");
		}
		if (i==1){
			strcpy(name, "Disk 2");
		}
        sprintf(buf, "%d", i);
        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            console("start3(): Can't create disk driver %d\n", i);
            halt(1);
        }
        if (i==0){
			disk1PID=pid;
		}
		if (i==1){
			disk2PID=pid;
		}
		disk_head[i]=0;
    }

	//
	// wait for the disk device drivers to run
	//
	semp_real(running);
	semp_real(running);

	//
	// find out the number of tracks on each disk
	// dummy var used because I dont care about those return vals
	//
	disk_size(0, &dummy, &dummy, &disk1ntrack);
	disk_size(1, &dummy, &dummy, &disk2ntrack);

    //
    // Create terminal device drivers.
    //

    //
    // create message boxes to store lines
    //
    for (i=0; i < TERM_UNITS; i++){
		termread_mbox[i] = MboxCreate(1, 0);
		termreadbuf_mbox[i] = MboxCreate(N_BUF_LINE, MAXLINE);
		termwrite_mbox[i] = MboxCreate(1, 0);
		termwritebuf_mbox[i] = MboxCreate(1, MAXLINE);
		termwritecomp_mbox[i] = MboxCreate(1, sizeof(int));
		term_mutex[i] = semcreate_real(1);
		writing[i]=FALSE;
	}

	writeline[0]=writeline0;
	writeline[1]=writeline1;
	writeline[2]=writeline2;
	writeline[3]=writeline3;

    for (i = 0; i < TERM_UNITS; i++) {
		//
		// put terminal names into "name"
		//
		if (i==0){
			strcpy(name, "Term 1");
		}
		if (i==1){
			strcpy(name, "Term 2");
		}
		if (i==2){
			strcpy(name, "Term 3");
		}
		if (i==3){
			strcpy(name, "Term 4");
		}
		sprintf(buf, "%d", i);
		pid = fork1(name, TermDriver, buf, USLOSS_MIN_STACK, 2);
		if (pid < 0) {
				console("start3(): Can't create term driver %d\n", i);
				halt(1);
		}
		pid_read = fork1(name, termdriver_read, buf, USLOSS_MIN_STACK, 2);
		if (pid < 0) {
				console("start3(): Can't create term driver %d\n", i);
				halt(1);
		}
		pid_write = fork1(name, termdriver_write, buf, USLOSS_MIN_STACK, 2);
		if (pid < 0) {
				console("start3(): Can't create term driver %d\n", i);
				halt(1);
		}
		//
		// remembers pid's so we can zap them later on
		//
      	if (i==0){
			term1PID[0]=pid;
			term1PID[1]=pid_read;
			term1PID[2]=pid_write;
		}
		if (i==1){
			term2PID[0]=pid;
			term2PID[1]=pid_read;
			term2PID[2]=pid_write;
		}
		if (i==2){
			term3PID[0]=pid;
			term3PID[1]=pid_read;
			term3PID[2]=pid_write;
		}
		if (i==3){
			term4PID[0]=pid;
			term4PID[1]=pid_read;
			term4PID[2]=pid_write;
		}
    } // end for loop

	//
	// wait for the terminal device drivers to run
	//
    semp_real(running);
    semp_real(running);
    semp_real(running);
    semp_real(running);

    semp_real(running);
    semp_real(running);
    semp_real(running);
    semp_real(running);

    semp_real(running);
    semp_real(running);
    semp_real(running);
    semp_real(running);
    //
    // Create first user-level process and wait for it to finish.
    // These are lower-case because they are not system calls;
    // system calls cannot be invoked from kernel mode.
    // I'm assuming kernel-mode versions of the system calls
    // with lower-case names.
    //
    pid = spawn_real("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    wait_real(&pid, &status);


	//
	// free up clock driver
	//
	zap(clockPID);

	//
	// free up disk1, then zap it
	//
	semv_real(disk1req_sem);
	zap(disk1PID);

	//
	// free up disk2, then zap it
	//
	semv_real(disk2req_sem);
	zap(disk2PID);

	//
	// free up terminals,
	// then zap them
	//
	for (i=0; i < 4; i++){
		MboxSend(termread_mbox[i], NULL, 0);
		MboxSend(termwrite_mbox[i], NULL, 0);

	}
	for (i=2; i >= 0; i--){
		zap(term1PID[i]);
		zap(term2PID[i]);
		zap(term3PID[i]);
		zap(term4PID[i]);
	}
}
//
// end start3 function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// ClockDriver
//
// description: wakes up sleeping processes after a specified amount
// of time has elapsed
//
static int
ClockDriver(char *arg_c)
{
    int result;
    int status;
	int current_time;
	int i;
	double elapsed_time;

    //
    // Let the parent know we are running and enable interrupts.
    //
    semv_real(running);
    psr_set(psr_get() | PSR_CURRENT_INT);

    while(! is_zapped()) {
			result = waitdevice(CLOCK_DEV, 0, &status);
		//
		// Compute the current time and wake up any processes
		// whose time has come.
		//
		 for ( i=0; i<MAXPROC; i++) {

			 if (ProcTable4[i].status == SLEEPING_BEAUTY) {
				 current_time = sys_clock();
				 elapsed_time = (current_time - ProcTable4[i].sleep_start_time) / 1000000;
				 if (elapsed_time >= ProcTable4[i].sleep_time) {
				 		ProcTable4[i].status =INUSE;
				 		semv_real(ProcTable4[i].psem);
				 }
			 }
		 } // end for loop
   } // end while loop
   return 0;
}
//
// end ClockDriver function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// DiskDriver
//
// description: dequeues requests using an elevator algorithm,
// then services the dequeued request
//
static int
DiskDriver(char *arg_c)
{
	void* arg = (void*)arg_c;
	int unit = atoi( (char *) arg); 	// Unit is passed as arg.

	//
	// let the parent know we are running and enable interrupts
	//
	semv_real(running);
	psr_set(psr_get() | PSR_CURRENT_INT);

	while (! is_zapped()){
		diskdriver_real(unit);
	}

  return 0;
}
//
// end DiskDriver function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// diskdriver_real
//
// description: dequeues and calls device output in order to
// service requests
//
void diskdriver_real(int unit){

	int mutex = semcreate_real(1);
	diskreq_ptr req;
	device_request usloss_req;
	int status;
	//
	// wait for a disk request
	//
	if (unit==0){
		semp_real(disk1req_sem);
	}
	if (unit==1){
		semp_real(disk2req_sem);
	}

	if (is_zapped()){
		return;
	}

	//////////////////////////
	//
	// begin mutex
	//
	semp_real(mutex);
	//
	//
	//

	//
	// get a request
	// dequeue from the Head of the Q starting at track 0
	//
	if (unit==0){
		req=dequeue(&disk1q, disk_head[unit]);
	}
	if (unit==1){
		req=dequeue(&disk2q, disk_head[unit]);
	}

	//
	// if req is NULL we need to re-start at track 0
	//
	if (req ==NULL) {
		disk_head[unit] = 0;

		if (unit==0){
			req=dequeue(&disk1q, disk_head[unit]);
		}
		if (unit==1){
			req=dequeue(&disk2q, disk_head[unit]);
		}
	}

	//
	// remember last track and sector serviced
	// (where is the head is)
	//
	disk_head[unit] = req->track;

	//
	// move head of disk to given track
	//
	usloss_req.opr = DISK_SEEK;
	usloss_req.reg1 = (void*)req->track;
	device_output(DISK_DEV, unit, &usloss_req);
	//
	// wait for disk interrupt
	//
	waitdevice(DISK_DEV, unit, &status);

	//
	// read or write to or from disk
	//
	if (req->action == READ){
		usloss_req.opr = DISK_READ;
	}
	if (req->action == WRITE){
		usloss_req.opr = DISK_WRITE;
	}
	usloss_req.reg1 = (void*)req->first;
	usloss_req.reg2 = (void*)req->buf;
	device_output(DISK_DEV, unit, &usloss_req);
	//
	// wait for disk interrupt
	//
	waitdevice(DISK_DEV, unit, &status);

	//
	//
	//
	semv_real(mutex);
	//
	// end mutex
	//////////////////////////

	//
	// free up the process waiting
	// on the disk
	//
	semv_real(ProcTable4[req->proc_slot].psem);
}
//
// end diskdriver_real function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// TermDriver
//
// description: performs waitdevice, and notifies the terminal
// read and write processes accordingly
//
static int TermDriver(char* arg_c){
	int rc;
	int count;
	int unit = atoi( (char *) arg_c);
	termread_done[unit]=FALSE;

	//
	// let the parent know we are running and enable interrupts
	//
	semv_real(running);
	psr_set(psr_get() | PSR_CURRENT_INT);

	count=0;
	while(!is_zapped()){

		if (! writing[unit]){
			bzero(writeline[unit], MAXLINE);
			writelen[unit]=MboxCondReceive(termwritebuf_mbox[unit], writeline[unit], MAXLINE);
			if (writelen[unit] > 0){
				writing[unit]=TRUE;
				write_i[unit]=0;
			}
		}

		//
		// enable interrupts
		//
		control[unit]=0;
		control[unit] = TERM_CTRL_RECV_INT(control[unit]);
		control[unit] = TERM_CTRL_XMIT_INT(control[unit]);
   		rc = device_output(TERM_DEV, unit, (void*)control[unit]);
		if (rc != DEV_OK){
			printf("ERROR: DEVICE OUTPUT\n");
			return -1;
		}

		//
		// wait for an interrupt to occur from the terminal device
		//
		rc = waitdevice(TERM_DEV, unit, &status[unit]);

		if (is_zapped()){
			break;
		}

		if (TERM_STAT_RECV(status[unit])==DEV_BUSY){
			//
			// wake up read process to read
			// a single character
			//
			MboxCondSend(termread_mbox[unit], NULL, 0);
		} else {
			termread_done[unit]=TRUE;
		}

		if (TERM_STAT_XMIT(status[unit])==DEV_READY){
			//
			// wake up write process to write
			// a single character
			//
			MboxCondSend(termwrite_mbox[unit], NULL, 0);
		}



	} // end while

	return 0;
}
//
// end TermDriver function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// termdriver_read
//
// description: reads a character into line, and sends the whole line
// into the terminal read mailbox when the whole line has been read
//
static int termdriver_read(char* arg){

	char readline[MAXLINE];
	int read_i;
	int unit = atoi((char*)arg);
	int ex =0;

	//
	// let the parent know we are running and enable interrupts
	//
	semv_real(running);
	psr_set(psr_get() | PSR_CURRENT_INT);

	read_i=0;
	while(! termread_done[unit]&& !is_zapped()){
		//
		// block until its OK to read
		//
		if (ex >=0){
			ex = MboxReceive(termread_mbox[unit], ZERO_BUF, 0);
		}

		if (read_i < MAXLINE) {
			//
			// read and add characer to the buffer
			//
			readline[read_i]=TERM_STAT_CHAR(status[unit]);

			//
			// see if we are done reading the line
			//
			if (readline[read_i]=='\n'){
				//
				// NEWLINE reached, copy, send, and zero out readline
				//
				strcpy(line_to_send, readline);
				MboxCondSend(termreadbuf_mbox[unit], (void*)line_to_send, read_i+1);
				bzero(readline, MAXLINE);
				read_i=0;
			} else {
				read_i++;
			}

		} else {
			//
			// MAXLINE reached, copy, send, and zero out readline
			//
			strcpy(line_to_send, readline);
			MboxCondSend(termreadbuf_mbox[unit], (void*)line_to_send, MAXLINE);
			bzero(readline, MAXLINE);
			read_i=0;
		}
	}

	return 0;
}
//
// end termdriver_read function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// termdriver_write
//
// description: writes a whole request character by character, then
// accepts another request (only 1 write request per terminal per time)
//
static int termdriver_write(char* arg_c){
	int unit = atoi(arg_c);
	int rc;

	//
	// let the parent know we are running and enable interrupts
	//
	semv_real(running);
	psr_set(psr_get() | PSR_CURRENT_INT);

	while(!is_zapped()){
		//
		// block until its OK to send
		//
		MboxReceive(termwrite_mbox[unit], NULL, 0);


		/////////////////////////
		//
		// begin mutex
		//
		semp_real(term_mutex[unit]);
		//
		//
		//


		//
		// if we are already writing, continue writing
		//
		if (writing[unit]){
			//
			// stick current character into control
			//
			control[unit]=TERM_CTRL_CHAR(control[unit], writeline[unit][write_i[unit]]);

			//
			// set transmit flag to write
			// the current character to terminal
			//
			control[unit]=TERM_CTRL_XMIT_CHAR(control[unit]);

			//
			// perform device output
			//
			rc=device_output(TERM_DEV, unit, (void*)control[unit]);
			if (rc != DEV_OK){
				printf("ERROR: DEVICE OUTPUT\n");
				return -1;
			}

			//
			// increment current write index
			//
			write_i[unit]++;

			if (write_i[unit]==writelen[unit]){
				writing[unit]=FALSE;
				write_i[unit]=0;
				MboxSend(termwritecomp_mbox[unit], NULL, 0);
			}

		}

		//
		//
		//
		semv_real(term_mutex[unit]);
		//
		// end mutex
		//
		/////////////////////////

	}

	return 0;
}
//
// end termdriver_write function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// kernel_sleep
//
// description: extracts arguments from sysargs, validates the
// arguments, then calls sleep_real if the arguments are valid
//
static void kernel_sleep(sysargs * sa) {

	int sleep_time;

	sleep_time = (int)sa->arg1;

	//
	// make sure the time to sleep is
	// a positive value
	//
	if (sleep_time <0)
		sa->arg4 = (void *)-1;
	else {
		sleep_real(sleep_time);
		sa->arg4 = (void *)0;
	}

	to_user_mode("kernel_sleep");
}
//
// end kernel_sleep function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// sleep_real
//
// description: keeps track of the amount of time to sleep (in seconds)
// keeps track of the current time, which the process starts "sleeping"
// sets the process status to SLEEPING_BEAUTY, and blocks on the processes
// private semaphore
//
static void sleep_real(int sleep_time) {
	int proc_slot;
	proc_slot = find_proc(getpid());

	if (proc_slot < 0) {
		proc_slot = addto_proctable(getpid());
		if (proc_slot < 0){
			terminate_real(-1);
		}
	}

	ProcTable4[proc_slot].sleep_time = sleep_time;
	ProcTable4[proc_slot].sleep_start_time = sys_clock();
	ProcTable4[proc_slot].status = SLEEPING_BEAUTY;
	semp_real(ProcTable4[proc_slot].psem);
}
//
// end sleep_real function
//////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
// kernel_diskread
//
// description: extracts and validates arguments from sysarg structure,
// then calls disk_read with those arguments
//
static void kernel_diskread(sysargs* sa){
	void* buf = sa->arg1;
	int sectors = (int)sa->arg2;
	int track = (int)sa->arg3;
	int first = (int)sa->arg4;
	int unit = (int)sa->arg5;
	int rc;

	sa->arg4=(void*)0;

	if (buf == NULL){
		sa->arg4=(void*)-1;
	}

	//
	// number of sectors to read
	//
	if (sectors <= 0
	||  sectors > DISK_TRACK_SIZE){
		sa->arg4=(void*)-1;
	}

	//
	// starting disk track number
	//
	if (track < 0){
		sa->arg4=(void*)-1;
	}
	if (unit==0){
		if (track >= disk1ntrack){
			sa->arg4=(void*)-1;
		}
	}
	if (unit==1){
		if(track >= disk2ntrack){
			sa->arg4=(void*)-1;
		}
	}

	//
	// starting disk sector number
	//
	if (first < 0
	||  first > DISK_TRACK_SIZE){
		sa->arg4=(void*)-1;
	}

	//
	// make sure the unit is valid
	// there are two units, 0 and 1
	//
	if (unit != 0 && unit != 1){
		sa->arg4=(void*)-1;
	}

	//
	// if all the arguments are OK,
	// then perform the disk read
	//
	if ((int)sa->arg4 == 0){
		rc=disk_read(unit, track, first, sectors, buf);
		if (rc == 0){
			sa->arg1 = (void*)0;
		} else {
			sa->arg1 = (void*)-1;
		}
	}
}
//
// end kernel_diskread function
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
// disk_read
//
// description: performs "sectors" reads from the disk, and stores
// the data into buf.  Reads from disk start at track "track",
// and sector "first"
//
int disk_read(int unit, int track, int first, int sectors, void* buf){
	int i;
	int proc_slot = find_proc(getpid());
	if (proc_slot < 0){
		proc_slot = addto_proctable(getpid());
		if (proc_slot < 0){
			terminate_real(-1);
		}
	}

	for (i=0; i < sectors; i++){
		//
		// put stuff into the proc table
		//
		ProcTable4[proc_slot].disk_req.next_req = NULL;
		ProcTable4[proc_slot].disk_req.proc_slot = proc_slot;

		//
		// find out which track and sector
		// to read from next
		//
		if (first + i >= DISK_TRACK_SIZE){
			//
			// start at the beginning of the next track
			//
			track++;
			first=0;
			ProcTable4[proc_slot].disk_req.track = track;
			ProcTable4[proc_slot].disk_req.first = first;
		} else {
			//
			// continue on current track
			//
			ProcTable4[proc_slot].disk_req.track = track;
			ProcTable4[proc_slot].disk_req.first = first + i;
		}

		//
		// increment buffer position a sector at a time
		//
		ProcTable4[proc_slot].disk_req.buf = buf + (i * DISK_SECTOR_SIZE);

		//
		// set operation to READ in order
		// to read from the disk into buf
		//
		ProcTable4[proc_slot].disk_req.action = READ;

		//
		// add this request into the queue
		//
		if (unit == 0){
			enqueue(&disk1q, &ProcTable4[proc_slot].disk_req);
			semv_real(disk1req_sem);
		}
		if (unit == 1){
			enqueue(&disk2q, &ProcTable4[proc_slot].disk_req);
			semv_real(disk2req_sem);
		}

		//
		// block until the disk read is complete
		//
		semp_real(ProcTable4[proc_slot].psem);
	}

	return 0;
}
//
// end disk_read function
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
// kernel_diskwrite
//
// description: extracts and validates arguments from sysargs structure
// then calls disk_write with those arguments
//
static void kernel_diskwrite(sysargs* sa){
	void* buf = sa->arg1;
	int sectors = (int)sa->arg2;
	int track = (int)sa->arg3;
	int first = (int)sa->arg4;
	int unit = (int)sa->arg5;
	int rc;

	sa->arg4=(void*)0;

	if (buf == NULL){
		sa->arg4=(void*)-1;
	}

	if (sectors <= 0
	||  sectors > DISK_TRACK_SIZE){
		sa->arg4=(void*)-1;
	}

	//
	// starting disk track number
	//
	if (track < 0){
		sa->arg4=(void*)-1;
	}
	if (unit==0){
		if (track >= disk1ntrack){
			sa->arg4=(void*)-1;
		}
	}
	if (unit==1){
		if(track >= disk2ntrack){
			sa->arg4=(void*)-1;
		}
	}

	if (first < 0
	||  first > DISK_TRACK_SIZE){
		sa->arg4=(void*)-1;
	}

	if (unit != 0 && unit != 1){
		sa->arg4=(void*)-1;
	}

	if ((int)sa->arg4 == 0){
		rc=disk_write(unit, track, first, sectors, buf);
		if (rc == 0){
			sa->arg1 = (void*)0;
		} else {
			sa->arg1 = (void*)-1;
		}
	}
}
//
// end kernel_diskwrite function
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
// disk_write
//
// description: writes "sectors" sectors of the data from buf
// onto the disk, starting at track "track" and sector "first"
//
int disk_write(int unit, int track, int first, int sectors, void* buf){
	int i;
	int proc_slot = find_proc(getpid());
	if (proc_slot < 0){
		proc_slot = addto_proctable(getpid());
		if (proc_slot < 0){
			terminate_real(-1);
		}
	}

	for (i=0; i < sectors; i++){
		//
		// put stuff into the proc table request structure
		//
		ProcTable4[proc_slot].disk_req.next_req = NULL;
		ProcTable4[proc_slot].disk_req.proc_slot = proc_slot;

		//
		// find out which track and sector to write to
		//
		if (first + i >= DISK_TRACK_SIZE){
			//
			// start at the beginning of the next track
			//
			track++;
			first=0;
			ProcTable4[proc_slot].disk_req.track = track;
			ProcTable4[proc_slot].disk_req.first = first;
		} else {
			//
			// continue on current track
			//
			ProcTable4[proc_slot].disk_req.track = track;
			ProcTable4[proc_slot].disk_req.first = first + i;
		}

		//
		// increment buffer position a sector at a time
		//
		ProcTable4[proc_slot].disk_req.buf = buf+ (i * DISK_SECTOR_SIZE);

		//
		// set the operation to WRITE in order
		// to write from buf onto the disk
		//
		ProcTable4[proc_slot].disk_req.action = WRITE;

		//
		// add this request into the queue
		//
		if (unit == 0){
			enqueue(&disk1q, &ProcTable4[proc_slot].disk_req);
			semv_real(disk1req_sem);
		}
		if (unit == 1){
			enqueue(&disk2q, &ProcTable4[proc_slot].disk_req);
			semv_real(disk2req_sem);
		}

		//
		// block until the disk write is complete
		//
		semp_real(ProcTable4[proc_slot].psem);
	}

	return 0;
}
//
// end disk_write function
////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////
//
// kernel_disksize
//
// description: calls disk_size and returns values into sysargs
// arg1 is set to the size in bytes of each sector
// arg2 is set to the number of sectors in a track
// arg3 is set to the number of tracks on the disk
// arg4 is set to the disk size function's return code
//
static void kernel_disksize(sysargs* sa) {
	int unit = (int)sa->arg1;
	int sector_size;
	int sector_no;
	int track_no;
	int rc;

	if (unit <0 || unit >1 )
		sa->arg4 = (void *)-1;
	else {
		rc =disk_size(unit, &sector_size, &sector_no, &track_no );
		sa->arg1 = (void *)sector_size;
		sa->arg2 = (void *)sector_no;
		sa->arg3 = (void *)track_no;
		sa->arg4 = (void *)rc;
	}
}
//
// end kernel_disksize function
////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////
//
// disk_size
//
// description: find out disk size (sector X track X disk)...
//
// returns the bytes per sector into "sector"
// returns the sectors per track into "track"
// returns the tracks per disk into "disk"
//
// returns 0 upon sucessful completion, -1 otherwise
//
int disk_size(int unit, int* sector, int* track, int* disk) {
	device_request  usloss_req;
	int status;
	int rc;

	//
	// set sector equal to the number of bytes
	// per sector of disk
	//
	*sector=DISK_SECTOR_SIZE;

	//
	// set track equal to the number of sectors
	// per track of disk
	//
	*track = DISK_TRACK_SIZE;

	//
	// set disk equal to the number of tracks
	// per disk
	//
	usloss_req.opr = DISK_TRACKS;
	usloss_req.reg1 = (void *)disk;
	rc=device_output(DISK_DEV,unit,&usloss_req);
	if (rc != DEV_OK){
		return -1;
	}
	rc=waitdevice(DISK_DEV,unit,&status);
	if (rc != DEV_OK){
		return -1;
	}
	return 0;
}
//
// end disk_size function
////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// kernel_termread
//
// description: extracts and validates arguments from sysargs
// then calls term_read if the arguments are valid
//
static void kernel_termread(sysargs* sa){
	char* buf = (char*)sa->arg1;
	int buf_sz = (int)sa->arg2;
	int unit = (int)sa->arg3;
	int nchar_read;

	//
	// validate arguments
	//
	if (buf==NULL){
		sa->arg4 = (void*)-1;
		return;
	}
	if (buf_sz <= 0){
		sa->arg4 = (void*)-1;
		return;
	}
	if (unit < 0 || unit > 3){
		sa->arg4 = (void*)-1;
		return;
	}

	//
	// arguments are valid,
	// perform read from terminal
	//
	nchar_read = term_read(unit, buf_sz, buf);

	sa->arg2 = (void*)nchar_read;
	sa->arg4 = (void*)0;
}
//
// end kernel_termread function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// term_read
//
// description: wait for a line from the terminal buffer, then take
// the line, and copy is contents into buf
//
int term_read(int unit, int size, char* buf){
	int nchar_read=0;
	char  recv_buf[MAXLINE];

	//
	// wait for the terminal to buffer a line
	// into its mailbox, then receive that line
	//
	bzero((void *)recv_buf,MAXLINE);
	bzero((void *)buf,size);
	nchar_read=MboxReceive(termreadbuf_mbox[unit], (void*)recv_buf, MAXLINE);

	//
	// Copy received line into buf, this is done so buf doesnt
	// point to the mailbox buffer, which will be re-initialized
	//
	memcpy(buf,recv_buf,size);

	//
	// return the number of character read
	//
	return strlen(buf);
}
//
// end term_read function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// kernel_termwrite
//
// description: extracts and validages sysargs, then calls term_write
// if the arguments are valid
//
static void kernel_termwrite(sysargs* sa){
	char* buf = (char*)sa->arg1;
	int buf_sz = (int)sa->arg2;
	int unit = (int)sa->arg3;
	int nchar_written;

	//
	// validate arguments
	//
	if (buf==NULL){
		sa->arg4 = (void*)-1;
		return;
	}
	if (buf_sz <= 0){
		sa->arg4 = (void*)-1;
		return;
	}
	if (unit < 0 || unit > 3){
		sa->arg4 = (void*)-1;
		return;
	}

	nchar_written = term_write(unit, buf_sz, buf);

	sa->arg2 = (void*)nchar_written;
	sa->arg4 = (void*)0;
}
//
// end kernel_termwrite function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// term_write
//
// description: adds a request onto the terminals queue by sending
// a message to its write message box.  Then waits for a message
// back from the terminal write done message box, which indicates
// that the message has been sucessfully written to the terminal
//
int term_write(int unit, int buf_sz,char* buf){

	int nchar_written=0;

	//
	// give the driver the line to write
	//
	MboxSend(termwritebuf_mbox[unit], (void*)buf, buf_sz);

	//
	// wait for the driver to complete the write
	//
	MboxReceive(termwritecomp_mbox[unit], (void*)nchar_written, sizeof(int));

	return buf_sz;
}
//
// end term_write function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// init_proc
//
// description: initializes the process in the process table at index
// "index", by first zeroing it out, setting its pid to -1, status to
// EMPTY, and initializing its private semaphore to count of 0
//
static void init_proc(int index){
	bzero(&ProcTable4[index], sizeof(struct proc_struct));
	ProcTable4[index].pid = -1;
	ProcTable4[index].status = EMPTY;
	ProcTable4[index].psem = semcreate_real(0);
}
//
// end init_proc function
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// find_proc
//
// description: given the process id, this function returns the process's
// index in the process table (aka proc_slot)
//
static int find_proc(int pid){
	int proc_slot=0;

	do{
		if (ProcTable4[proc_slot].pid == pid){
			if (pid == -1){
				if(ProcTable4[proc_slot].status == INUSE){
					break;
				}
			} else {
				break;
			}
		}
		proc_slot++;
	}while(proc_slot < MAXPROC);

	if (proc_slot == MAXPROC){
		return -1;
	}

	return proc_slot;
}
//
// end find_proc function
//////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// addto_proctable
//
// description: adds process with given pid to the process table
// by finding an empty slot, and initializing its pid to given pid
// and setting its status to INUSE
//
static int addto_proctable(int pid){
	int child_slot;

	//////////////////////////
	// begin critical section
	//
	MboxSend(ProcTable_mbox, NULL, 0);
	//
	//
	//

	//
	// find an empty process table slot
	//
	child_slot=0;
	do{
		if (ProcTable4[child_slot].status==EMPTY){
			break;
		}
		child_slot++;
	}while(child_slot < MAXPROC);

	if (child_slot == MAXPROC){
		//
		// there is no room for the process
		//
		return -1;
	}

	//
	// intialize attributes
	//
	ProcTable4[child_slot].status = INUSE;
	ProcTable4[child_slot].pid = pid;

	//
	//
	//
	MboxReceive(ProcTable_mbox, ZERO_BUF, 0);
	//
	// end critical section
	////////////////////////

	return child_slot;
}
//
// end addto_proctable
///////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
//
// to_user_mode
//
// description: changes mode to user mode
//
void to_user_mode(char* func_name){
	if (PSR_CURRENT_MODE & psr_get()){
		int mask = 0xE; // 1110
		psr_set(psr_get() & mask);
	} else {
		console("already in user mode.\n");
	}
}
//
// end to_user_mode function
//////////////////////////////////////////////////////////////////////




