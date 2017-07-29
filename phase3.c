#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <strings.h>
#include <user.h>
#include <queue.h>
#include <stdio.h>
#include <usyscall.h>
#include <libuser.h>

/* --------------------------------- tests ---------------------------------
test00 -- OK
test01 -- OK
test02 -- OK
test03 -- OK
test04 -- OK
test05 -- OK
test06 -- OK
test07 -- OK
test08 -- OK
test09 -- OK
test10 -- OK
test11 -- OK
test12 -- OK
test13 -- OK
test14 -- OK
test15 -- OK
test16 -- OK
test17 -- OK
test18 -- OK
test19 -- OK
test20 -- OK
test21 -- OK
test22 -- OK
test23 -- OK
test24 -- OK
test25 -- OK
*/

/* --------------------------------- globals --------------------------------- */

//
// process table mailbox used as a mutex when processes attempt to find empty
// process slots and mark them as INUSE
//
int ProcTable_mbox;

//
// process table, stores MAXPROC processes
//
struct proc_struct ProcTable[MAXPROC];

//
// system call vector, points to handlers of system calls
// unknown system calls point to nullsys3
//
extern void (*sys_vec[MAXSYSCALLS])(sysargs *args);

//
// the user mode function which is spawned and
// waited for in start2
//
extern int start3(void*);

//
// dummy variable used for syntatic correctness
// when performing a MboxReceive in which the
// message received contains no data (messages used
// for synchonization only)
//
void* ZERO_BUF;

//
// semaphore table, stores MAXSEMS semaphores
//
semaphore SemTable[MAXSEMS];

//
// semaphore mailbox, used as a mutex when processes attempt to find
// empty semaphore slots and mark them as INUSE
//
int SemTable_mbox;

//
// number of semaphores
//
int numSems=0;


/* --------------------------------- prototypes --------------------------------- */
static void kernel_spawn(sysargs* sa);
int spawn_real(char* name, int (*func)(void*), void* arg, int stack_size, int priority);
static int spawn_launch(char* dummy);
static void kernel_wait(sysargs* sa);
void wait_real(int* pid, int* status);
static void kernel_terminate(sysargs* sa);
void terminate_real(int status);
static void kernel_semcreate(sysargs* sa);
int semcreate_real(int initval);
static void kernel_semp(sysargs* sa);
void semp_real(int semindex);
static void kernel_semv(sysargs* sa);
void semv_real(int semindex);
static void kernel_semfree(sysargs* sa);
int semfree_real(int semindex);
static void kernel_getpid(sysargs* sa);
int getpid_real();
static void kernel_daytime(sysargs* sa);
static void kernel_cputime(sysargs* sa);
static void init_proc(int index);
static int find_proc(int pid);
static int addto_proctable(int pid);
static void init_sem(int i);
static int find_empty_sem(void);
static void to_user_mode(char* func_name);
void nullsys3(sysargs* sa);

/* --------------------------------- functions --------------------------------- */

/* ------------------------------------------------------------------------
   Name - check_kernel_mode
   Purpose - The purpose of the confirm_kernelmode is to make sure that
   	   the current mode is kernel mode
   Parameters - none
   Returns - 1 if the current mode is kernelmode
             0 otherwise
   Side Effects -  none (hopefully)
   ----------------------------------------------------------------------- */
void check_kernel_mode(char* func_name)
{
	if(! PSR_CURRENT_MODE & psr_get()){
		console("%s(): not in kernel mode.\n");
		halt(1);
	}

} /* confirm_kernelmode */


///////////////////////////////////////////////////////////////
//
// start2
//
int
start2(void *arg)
{
	int pid;
	int status;
	int i;

	//
	// make sure we are in kernel mode
	//
	check_kernel_mode("start2");

	//
	// intitialize the ProcTable
	//
	for (i=0; i < MAXPROC; i++){
		init_proc(i);
	}

	//
	// initialize the sys_vec
	//
	for (i=0; i < MAXSYSCALLS; i++){
		sys_vec[i]=nullsys3;
	}
	sys_vec[SYS_SPAWN]=kernel_spawn;
	sys_vec[SYS_WAIT]=kernel_wait;
	sys_vec[SYS_TERMINATE]=kernel_terminate;
	sys_vec[SYS_SEMCREATE]=kernel_semcreate;
	sys_vec[SYS_SEMP]=kernel_semp;
	sys_vec[SYS_SEMV]=kernel_semv;
	sys_vec[SYS_SEMFREE]=kernel_semfree;
	sys_vec[SYS_GETTIMEOFDAY]=kernel_daytime;
	sys_vec[SYS_CPUTIME]=kernel_cputime;
	sys_vec[SYS_GETPID]=kernel_getpid;

	//
	// inititialize the SemTable
	//
	for (i=0; i < MAXSEMS; i++){
		init_sem(i);
	}

	//
	// create process table mutex mailbox
	// (one slot, zero sized mesg)
	//
	ProcTable_mbox = MboxCreate(1, 0);

	//
	// create semaphore table mutex mailbox
	// (one slot, zero sized mesg)
	//
	SemTable_mbox =MboxCreate(1,0);

	/*
	 * Create first user-level process and wait for it to finish.
	 * These are lower-case because they are not system calls;
	 * system calls cannot be invoked from kernel mode.
	 * Assumes kernel-mode versions of the system calls
	 * with lower-case names.  I.e., Spawn is the user-mode function
	 * called by the test cases; spawn is the kernel-mode function that
	 * is called by the syscall_handler; spawn_real is the function that
	 * contains the implementation and is called by spawn.
	 *
	 * Spawn() is in libuser.c.  It invokes usyscall()
	 * The system call handler calls a function named spawn() -- note lower
	 * case -- that extracts the arguments from the sysargs pointer, and
	 * checks them for possible errors.  This function then calls spawn_real().
	 *
	 * Here, we only call spawn_real(), since we are already in kernel mode.
	 *
	 * spawn_real() will create the process by using a call to fork1 to
	 * create a process executing the code in spawn_launch().  spawn_real()
	 * and spawn_launch() then coordinate the completion of the phase 3
	 * process table entries needed for the new process.  spawn_real() will
	 * return to the original caller of Spawn, while spawn_launch() will
	 * begin executing the function passed to Spawn. spawn_launch() will
	 * need to switch to user-mode before allowing user code to execute.
	 * spawn_real() will return to spawn(), which will put the return
	 * values back into the sysargs pointer, switch to user-mode, and
	 * return to the user code that called Spawn.
	 */

	//
	// add myself to the process table
	//
	addto_proctable(getpid());

	//
	// spawn start3 and wait for it to finish
	//
	pid = spawn_real("start3", start3, NULL,  32 * 1024, 3);
	wait_real(&pid, &status);
	return 0;
}
//
// end start2
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// kernel_spawn
//
// description: extracts args, validates them, then calls
// spawn_real to perform the real spawning
//
static void kernel_spawn(sysargs* sa){

	int kid_pid;

	char* name;
	int (*func)(void*);
	void* arg;
	int stack_size;
	int priority;

	//
	// extract params from sysargs
	//
	func = sa->arg1;
	arg = sa->arg2;
	stack_size = (int)sa->arg3;
	priority = (int)sa->arg4;
	name = (char*)sa->arg5;

	//
	// validate parameters
	//
	if ((func==NULL)
	|| (stack_size < USLOSS_MIN_STACK)
	|| (priority < HIGHEST_PRIORITY || priority > LOWEST_PRIORITY)
	|| (name==NULL)){

		sa->arg1 = (void*)-1;
		sa->arg4 = (void*)-1;
	} else {
		//
		// perform actual spawning of the process
		//
		kid_pid = spawn_real(name, func, arg, stack_size, priority);
		if (kid_pid < 0){
			kid_pid = -1;
		}

		//
		// put returns values into sysargs
		//
		sa->arg1 = (void*)kid_pid;
		sa->arg4 = 0;
	}

	//
	// switch to user mode
	//
	to_user_mode("kernel_spawn");
}
//
// end kernel_spawn
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// spawn_real
//
// description: performs fork1 to create a new process, then
// intializes the process in conjunction with spawn_launch
//
int spawn_real(char *name, int (*func)(void *), void *arg, int stack_size,
	int priority){

	int kid_pid;
	int child_slot;
	int parent_slot;
	proc_ptr temp;
	int kidCreated = FALSE;

	//
	// create new process
	//
	kid_pid = fork1(name, spawn_launch, NULL, stack_size, priority);
	//
	// the child will run first in spawn_launch after this call
	// if not, the parent will block and allow it to run first
	//

	if (kid_pid >= 0) { // Only if Fork was successfull


		// Figure out if the parent or the child initilized the child first
		// so we can decide if we need to unblock the child
		//
		child_slot = find_proc(kid_pid);

		if (child_slot < 0)
			child_slot = addto_proctable(kid_pid);
		else // The child initilized itself
			kidCreated =TRUE;

		//
		// finish initializing the child
		//
		child_slot = find_proc(kid_pid);
		if (child_slot < 0){
			printf("spawn_real() CHILD FIND ERROR: find_proc() could not find pid = %d", kid_pid);
			terminate_real(-1);
		}
		ProcTable[child_slot].start_func=func;
		ProcTable[child_slot].start_arg=arg;

		//Adding Child to the Parent's Child List
		//
		parent_slot = find_proc(getpid());
		temp = &ProcTable[parent_slot];
		enqueue_child(&temp,&ProcTable[child_slot]);


		//
		// unblock the child
		// if we get in here it means that the child ran first
		// therefore blocked itself and now we need to unblock it
		//
		if (kidCreated)
			MboxSend(ProcTable[child_slot].private_mbox, ZERO_BUF, 0);

	}

	//
	// return the newly created process's id
	//
	return kid_pid;
}
//
// end spawn_real
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// void spawn_launch(void) would work, but fork1 requires
// this bad boy to take a char* param, and return an int
//
static int spawn_launch(char* dummy){
	int curr_slot;
	proc_ptr curr_proc;
	int rc;
	int block_child =TRUE;

	//
	// see if I've already been initialized
	// if I have, then I don't need to block
	// if I am not initialized, then block
	// and allow my parent to wake me up
	// once I am initialized
	//
	curr_slot = find_proc(getpid());

	if (curr_slot < 0){
		//
		// add myself to the process table
		// if I am not already there
		//
		curr_slot = addto_proctable(getpid());
	}else{
		//
		// Parent Already initilized me so I don't
		// need to block
		//
		block_child =FALSE;
	}


	//
	// block myself so my parent can finish setting me up
	//
	if (block_child) {
		ProcTable[curr_slot].status=BLOCKED;
		MboxReceive(ProcTable[curr_slot].private_mbox, ZERO_BUF, 0);
		ProcTable[curr_slot].status=INUSE;
	}

	//
	// If I was zapped don't run, just quit
	//
	if (is_zapped()) {
		terminate_real(-1);
	}

	//
	// switch to user mode
	//
	to_user_mode("spawn_launch");

	//
	// allow user to execute code
	//
	curr_proc = &ProcTable[curr_slot];
	rc = curr_proc->start_func(curr_proc->start_arg);

	//
	// call terminate to terminate user code
	//
	Terminate(rc);

	return 0; // so GCC doesnt complain
}
//
// end spawn_launch
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// kernel_wait
//
// description: calls wait_real, which performs a join
// waiting for a child to quit, then returns the
// pid of the quit child into sysargs1, and returns
// the status of the quit child into sysargs2
//
static void kernel_wait(sysargs* sa){
	int pid;
	int status;

	//
	// perform real wait, and get
	// return vals
	//
	wait_real(&pid, &status);

	//
	// return pid and status into sysargs
	//
	sa->arg1=(void*)pid;
	sa->arg2=(void*)status;
	if (pid == -2){
		sa->arg4=(void*)-1;
	} else {
		sa->arg4=0;
	}

	//
	// switch to user mode
	//
	to_user_mode("kernel_wait");
}
//
// end kernel_wait
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// wait_real
//
// description: performs a join, which will block the current
// process until one of its children has quit
//
void wait_real(int* pid, int* status){
	//
	// wait for a child to die,
	// then returns its pid and status
	//
	*pid=join(status);
}
//
// end wait_real
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// kernel_terminate
//
// description: extracts status and calls terminate_real
//
static void kernel_terminate(sysargs* sa){
	int status;
	status = (int)sa->arg1;
	terminate_real(status);

	//
	// switch to user mode
	//
	to_user_mode("kernel_terminate");
}
//
// end kernel_terminate
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// terminate_real
//
// description: zaps current processes active children
// which causes the current process to block until all of its
// children have quit, then quits itself
//
void terminate_real(int status){

	int child_pid;
	int parent_slot;
	int child_slot;

	parent_slot = find_proc(getpid());
	//
	// Find everyone in the parent's child list that hasn't
	// terminated and zap them so we can terminate them in
	// spawn launch before they run
	//
	while (1) {
		child_pid = remove_child(&ProcTable[parent_slot]);
		if (child_pid == -1)
			break;

		child_slot = find_proc(child_pid);

		if (ProcTable[child_slot].status != TERMINATED){
			zap(child_pid);
		}
	}

	//
	// now quit
	//
	ProcTable[parent_slot].status =TERMINATED;
	quit(status);
}
//
// end terminate_real
/////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_semcreate
//
// description: checks sysargs for validity, then
// creates the semaphore by calling semcreate_real
//
static void kernel_semcreate(sysargs* sa){

	int initval;

	//
	// extract initial value
	//
	initval = (int)sa->arg1;

	//
	// make sure init. val
	// is >= 0
	//
	if (initval <0) {
		sa->arg4 = (void *)-1;
	}
	else {
		/////////////////////
		// begin mutex
		//
		MboxSend(SemTable_mbox,NULL,0);
		//
		//
		//

		//
		// make sure there is room
		// for another semaphore
		//
		if (numSems >=MAXSEMS) {
			sa->arg4 = (void *)-1;
		}
		else {
			//
			// if there is room,
			// create semaphore
			//
			sa->arg1 = (void *)semcreate_real(initval);
			sa->arg4 = (void *)0;
		}

		//
		//
		//
		MboxReceive(SemTable_mbox,ZERO_BUF,0);
		//
		// end mutex
		/////////////////////

	}
	//
	// switch to user mode
	//
	to_user_mode("kernel_semcreate");
}
//
// kernel_semcreate
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// semcreate_real
//
// description: creates a new semaphore by finding an
// empty slot in the semaphore table, and setting it up
//
int semcreate_real(int initval){

	int sem_slot;

	//
	// call find_empty_sem, which will
	// find an empty slot in the sem table
	// and set its status to INUSE
	//
	sem_slot = find_empty_sem();

	//
	// check to see if there is enough
	// room in the semaphore table
	// to create another semaphore
	//
	if (sem_slot <0){
		return -1;
	}

	//
	// set the ID of the semaphore
	// to its index in the table
	//
	SemTable[sem_slot].sid = sem_slot;

	//
	// set the semaphores count to
	// the initial value
	//
	SemTable[sem_slot].count = initval;

	//
	// keep track of the total number of
	// semaphores
	//
	numSems++;

	//
	// return the index of the semaphore
	// in the semaphore table
	//
	return sem_slot;
}
//
// end semcreate_real
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_semp
//
// description: makes sure the sem id is valid,
// then calls semp_real to perform the "P" operation
// on the semaphore
//
// returns sysarg4 0 if the semaphore handle is valid
// returns sysarg4 -1 if the semaphore handle is invalid
//
static void kernel_semp(sysargs* sa) {
	int semindex;

	semindex = (int)sa->arg1;

	if (semindex < 0 || semindex >=MAXSEMS)
		sa->arg4= (void *)-1;
	else {
		semp_real(semindex);
		sa->arg4= (void *)0;
	}

	//
	// switch to user mode
	//
	to_user_mode("kernel_semp");
}
//
// end kernel_semp
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// semp_real
//
// description: performs the "P" operation on the semaphore
// with sem id "semindex"
//
void semp_real(int semindex){

	int proc_slot;
	int block=FALSE;

	//
	// find the process slot for the current
	// process
	//
	proc_slot = find_proc(getpid());

	//////////////////
	//
	// begin mutex
	//
	MboxSend(SemTable[semindex].mutex_mbox,NULL,0);
	//
	//
	//

	//
	// block is the count is 0
	//
	if (SemTable[semindex].count ==0) {
		//
		// Add process to the Semaphore's block List
		//
		add_proc(&SemTable[semindex].block_list,&ProcTable[proc_slot]);
		//
		// remember to block, once out of mutex
		//
		block=TRUE;
	}
	else {
		//
		// otherwise, decrement count and
		// allow process to continue
		//
		SemTable[semindex].count--;
	}

	//
	//
	//
	MboxReceive(SemTable[semindex].mutex_mbox,ZERO_BUF,0);
	//
	// end mutex
	//
	////////////////

	//
	// if the count was 0, then the current
	// process will block itself now that it
	// is outside of the mutex
	//
	if (block == TRUE){
		//
		// Block Current process
		//
		MboxReceive(ProcTable[proc_slot].private_mbox,ZERO_BUF,0);
		//
		// check to see if the semaphore was freed
		//
		if (SemTable[semindex].status == FREE_WILLY) {
			//
			// if the semaphore was free while I was
			// waiting on it, terminate myself
			//
			terminate_real(1);
		}
	}
}
//
// end semp_real
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_semv
//
// description: checks to make sure the semaphore id is
// a valid semaphore id, then calls semv_real to
// perform the "V" operation on the semaphore
//
// returns sysarg4 0 if the semaphore handle is valid
// returns sysarg4 -1 if the semaphore handle is invalid
//
static void kernel_semv(sysargs* sa) {
	int semindex;

	semindex = (int)sa->arg1;

	if (semindex < 0 || semindex >=MAXSEMS)
		sa->arg4= (void *)-1;
	else {
		semv_real(semindex);
		sa->arg4= (void *)0;
	}

	//
	// switch to user mode
	//
	to_user_mode("kernel_semv");
}
//
// end kernel_semv
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// semv_real
//
// description: performs a "V" operation on the semaphore
// whose semaphore identifer "semindex" is given as a
// parameter
//
void semv_real(int semindex){

	int proc_slot;
	proc_ptr blocked_proc;

	////////////////////////
	// begin mutex
	//
	MboxSend(SemTable[semindex].mutex_mbox,NULL,0);
	//
	//
	//

	blocked_proc = remove_proc(&SemTable[semindex].block_list);

	if (blocked_proc == NULL){
		//
		// increment count if there are no
		// processes blocked on the semaphore
		//
		SemTable[semindex].count++;
	}
	else {
		//
		// release a process blocked on the
		// semaphore and do not modify count
		//
		proc_slot = find_proc(blocked_proc->pid);
		MboxSend(ProcTable[proc_slot].private_mbox,NULL,0);
	}

	//
	//
	//
	MboxReceive(SemTable[semindex].mutex_mbox,ZERO_BUF,0);
	//
	// end mutex
	////////////////////////
}
//
// end semv_real
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////
//
// kernel_semfree
//
// description: extracts arguments from sa, and calls semfree_real
//
static void kernel_semfree(sysargs* sa){
	int semindex;
	int rv;

	semindex = (int)sa->arg1;
	if (semindex < 0 || semindex >=MAXSEMS)
		sa->arg4= (void *)-1;
	else {
		rv = semfree_real(semindex);
		sa->arg4= (void *) rv;
	}

	//
	// switch to user mode
	//
	to_user_mode("kernel_semfree");
}
//
// end kernel_semfree
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// semfree_real
//
// description: unblocks all processes waiting in the
// semaphore, they will then terminate theirselves
// in semp_real, by checking to see if the semaphore
// status is FREE_WILLY (first thing checked once unblocked)
//
int semfree_real(int semindex){
	proc_ptr block_proc;
	int   proc_flag = TRUE;
	int   proc_slot;

	SemTable[semindex].status =FREE_WILLY;

	block_proc = remove_proc(&SemTable[semindex].block_list);

	if (block_proc == NULL)
		proc_flag = FALSE;
	else {
		do {

			proc_slot = find_proc(block_proc->pid);
			MboxSend(ProcTable[proc_slot].private_mbox,NULL,0);
			block_proc = remove_proc(&SemTable[semindex].block_list);

		} while (block_proc != NULL);
	}

	SemTable[semindex].status =EMPTY;
	numSems--;

	return proc_flag;
}
//
// end semfree_real
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_getpid
//
// description: returns into sysargs1 the process id
// of the current process
//
static void kernel_getpid(sysargs* sa){
	int pid;
	pid=getpid_real();
	sa->arg1=(void*)pid;

	//
	// switch to user mode
	//
	to_user_mode("kernel_getpid");
}
//
// end kernel_getpid
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// getpid_real
//
// description: returns the process identifier of the
// current process
//
int getpid_real(){
	return getpid();
}
//
// end getpid_real
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_daytime
//
// description: returns into sysargs1 time of day
// from the USLOSS sysclock
//
static void kernel_daytime(sysargs* sa){
	int daytime=sys_clock();
	sa->arg1=(void*)daytime;

	//
	// switch to user mode
	//
	to_user_mode("kernel_daytime");
}
//
// end kernel_daytime
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// kernel_cputime
//
// description: returns into sysarg1 the cpu time consumed
// by the current process
//
static void kernel_cputime(sysargs* sa){
	int cputime;
	cputime=readtime();
	sa->arg1=(void*)cputime;

	//
	// switch to user mode
	//
	to_user_mode("kernel_cputime");
}
//
// end kernel_cputime
////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// init_proc
//
// description: initializes the process at given index
// in the process table
//
static void init_proc(int index){
	bzero(&ProcTable[index], sizeof(struct proc_struct));
	ProcTable[index].pid = -1;
	ProcTable[index].status = EMPTY;
	ProcTable[index].private_mbox = MboxCreate(0, 0);
	ProcTable[index].mutex_mbox = MboxCreate(1, 0);
}
//
// end init_proc
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// find_proc
//
// description: searches proc table for a process with given
// pid, if found, returns the process index in the process table
// otherwise returns -1
//
static int find_proc(int pid){
	int proc_slot=0;

	do{
		if (ProcTable[proc_slot].pid == pid){
			if (pid == -1){
				if(ProcTable[proc_slot].status == INUSE){
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
// end find_proc
///////////////////////////////////////////////////////////////


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
		if (ProcTable[child_slot].status==EMPTY){
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
	ProcTable[child_slot].status = INUSE;
	ProcTable[child_slot].pid = pid;

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


///////////////////////////////////////////////////////////////
//
// init_sem
//
// description: Initialize Semaphore Table for the given Index
//
static void init_sem(int i){
	bzero(&SemTable[i], sizeof(semaphore));
	SemTable[i].status=EMPTY;
	SemTable[i].sid = i;
	SemTable[i].mutex_mbox = MboxCreate(1,0);
}
//
// end init_sem
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// find_empty_sem
//
// description: Find an empty Space in the semaphore table
// and mark it as INUSE
//
static int find_empty_sem(void) {

	int i=0;

	do {
		if (SemTable[i].status==EMPTY)
			break;

		i++;
	}while (i<MAXSEMS);

	//Check if the sem Table is full
	if (i==MAXSEMS)
		return -1;


	SemTable[i].status=INUSE;

	return i;
}
//
// end find_empty_sem
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// to_user_mode
//
// description: changes mode to user mode
//
static void to_user_mode(char* func_name){
	if (PSR_CURRENT_MODE & psr_get()){
		int mask = 0xE; // 1110
		psr_set(psr_get() & mask);
	} else {
		console("already in user mode.\n");
	}
}
//
// end to_user_mode
///////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////
//
// nullsys3
//
// description: phase3 version of nullsys -- invalid syscall
//
void nullsys3(sysargs* sa){
	console("nullsys3(): Invalid syscall. Terminating...\n");
	terminate_real(-1);
}
//
// end nullsys3
///////////////////////////////////////////////////////////////
