/* ------------------------------------------------------------------------
   kernel.c

   University of Arizona
   Computer Science 452
   Summer 2003

	MASTER MINDS: Boris Salazar and Clayton Wong

   ------------------------------------------------------------------------ */

#include <phase1.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <kernel.h>
#include <queue.h>


/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void disableInterrupts(void);
static void enableInterrupts(void);
static void check_deadlock(void);
int confirm_kernelmode(void);
void dump_spec_procs(int flag);
void dump_processes(void);
void dump_ready_list(void);
char* str_status(int status);
int zap(int pid);
int is_zapped(void);
proc_ptr get_process_ptr(int pid);
int nproc(void);
int block_me(int new_status);
int unblock_proc(int pid);
void int_handler(int int_dev, int unit);
int readtime(void);
int read_cur_start_time(void);
void track_proc_start();
void track_proc_end();
int stackallign(int memsize);
static void init_proc(int index);
void cleanup(void);
void time_slice(void);

/* -------------------------- Globals ------------------------------------- */

/* Patrick's debugging global variable... */
int debugflag = 0;

/* the process table */
static proc_struct ProcTable[MAXPROC];

/* Process lists  */
proc_ptr ready_list[LOWEST_PRIORITY+1];

/* current process ID */
proc_ptr Current;

/* the next pid to be assigned */
unsigned int next_pid = SENTINELPID;
unsigned int last_pid = -1;

// start & end times to keep track of process cpu time
unsigned int start;
unsigned int end;


/* -------------------------- Functions ----------------------------------- */


/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	     Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
  int i;      /* loop index */
  int result; /* value returned by call to fork1() */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("startup(): called while in user mode. Halting... \n");
		halt(1);
	}

	//
	// initialize the process table
	//
	for (i=0; i < MAXPROC; i++){
		init_proc(i);
	}

	// Initialize the Ready list
	// ready_list size is the size of a proc_ptr * the amount of rows
	// in the ready list table, which is 1,2,...,MINPRIORITY
	bzero(&ready_list, sizeof(proc_ptr) * (LOWEST_PRIORITY+1));

	//
	// initially there is no current process
	//
	Current=NULL;

	//
	// Initialize the clock interrupt handler
	//
	int_vec[CLOCK_DEV]=int_handler;

	//
	// startup a sentinel process
	//
	result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
								 SENTINELPRIORITY);
	if (result < 0) {
		console("startup(): fork1 for sentinel returned an error, halting...\n");
		halt(1);
	}

	//
	// start the test process
	//
	result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
	if (result < 0) {
		console("startup(): fork1 for start1 returned an error, halting...\n");
		halt(1);
	}

	//
	// this code should not get executed...
	//
	console("startup(): Should not see this message! ");
	console("Returned from fork1 call that created start1\n");
	return;

} /* startup */


/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(){
} /* finish */


/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority)
{
	int   counter;		/* keeps track number of loop iterations */
	int   proc_slot;	/* maps process table index from pid */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		if (Current ==NULL){
			printf("fork1(): called while in user mode. Halting... \n");
		} else {
			printf("fork1(): called while in user mode, by process %d. Halting... \n",
				Current->pid);
		}
		halt(1);
	}

	////////////////////////////////////////////////////////////////////
	// BEGIN CRITICAL SECTION
	//
	disableInterrupts();

	//
	// Return if stack size is too small
	//
	if (stacksize < USLOSS_MIN_STACK){
		return -2;
	}

	//
	// round stack size to nearest multiple of USLOSS_MIN_STACK
	//
	stacksize=stackallign(stacksize);

	//
	// find an empty slot in the process table
	//
	counter=0;
	do{
		//
		// next_pid always increments, when
		// next_pid is modded by MAXPROC, we
		// are able to map next_pid onto the
		// process table [0 to MAXPROC-1] inclusive
		//
		proc_slot=next_pid%(MAXPROC);
		if (next_pid == last_pid){
			//
			// do not use recently used pid's
			//
			next_pid++;
			continue;
		}
		if (ProcTable[proc_slot].status == EMPTY){
			//
			// found an empty slot in the process
			// table, exit loop
			//
			break;
		}
		//
		// increment next_pid and counter
		// to linearly search for an empty
		// slot in the process table
		//
		next_pid++;
		counter++;
	} while (counter < MAXPROC) ;

	//
	// keep track of the last pid used
	// in order to not use it again
	//
	last_pid=next_pid;

	//
	// return -1 if No empty slot available
	//
	if (counter == MAXPROC){
		return -1;
	}

	//
	// return -1 if priority is out of range
	//
	if ( priority > LOWEST_PRIORITY
	|| 	 priority < MAXPRIORITY){
		return -1;
	}

	//
	// return -1 if the function is NULL
	//
	if (f ==NULL){
		return -1;
	}

	//
	// fill-in entry in process table
	//
	if ( strlen(name) >= (MAXNAME - 1) ) {
		console("fork1(): Process name is too long.  Halting...\n");
		halt(1);
	}
	strcpy(ProcTable[proc_slot].name, name);
	ProcTable[proc_slot].start_func = f;
	if ( arg == NULL )
		ProcTable[proc_slot].start_arg[0] = '\0';
	else if ( strlen(arg) >= (MAXARG - 1) ) {
		console("fork1(): argument too long.  Halting...\n");
		halt(1);
	}
	else
		strcpy(ProcTable[proc_slot].start_arg, arg);

	//
	// allocate space for the stack, ProcTable[proc_slot].stack will
	// point to the lower address of the allocated space
	//
	ProcTable[proc_slot].stack=(char*)malloc(stacksize * sizeof(char));

	//
	// set stack to point to the high address of the
	// allocated space
	//
	ProcTable[proc_slot].stack+=stacksize * sizeof(char);

	//
	// set priority of process
	//
	ProcTable[proc_slot].priority=priority;

	//
	// Initialize context for this process, but use launch function pointer for
	// the initial value of the process's program counter (PC)
	//
  context_init(&(ProcTable[proc_slot].state), psr_get(),
  	ProcTable[proc_slot].stack, launch);

	//
	// set pid of process
	//
	ProcTable[proc_slot].pid = next_pid;

	//
	// set status of child process to ready
	//
	ProcTable[proc_slot].status = READY;

	//
	// add to the end of the ready list of same priority
	//
	enqueue(&ready_list[priority],&ProcTable[proc_slot]);

	//
	// keep track of the Parent
	//
	ProcTable[proc_slot].parent_proc_ptr = Current;

	//
	// Keep track of the parent's children
	//
	enqueue_child(&Current, &ProcTable[proc_slot]);

	//
  // for future phase(s)
  //
  p1_fork(ProcTable[proc_slot].pid);

	enableInterrupts();
	//
	// END CRITICAL SECTION
	////////////////////////////////////////////////////////////////////

	//
	// call dispatcher only if this new process
	// is not the sentinel
	//
	if (priority != LOWEST_PRIORITY){
		dispatcher();
	}

	//
	// return process id of the newly
	// created process
	//
	return next_pid;

} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
	int result;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("launch(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
  // Enable interrupts so the OS does not
  // loose control of the cpu to the user
  //
  enableInterrupts();

	//
  // Call the function passed to fork1, and capture its return value
  //
  result = Current->start_func(Current->start_arg);

	//
	// quit with result returned from start function
	//
  quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code)
{
	int cpid;							/* child's process id */
	proc_ptr killedChild;	/* used to re-init space */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("join(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
	// return -2 if the current process
	// has no children (note: children
	// are either on the child list, or
	// on the child kill list)
	//
	if (Current->child_proc_ptr==NULL
	&&  Current->child_kill_list==NULL){
		return -2;
	}

	//
	// block when the parent is
	// waiting for the child to
	// call quit
	//
	while(1){
		if (Current->child_kill_list==NULL){
			//
			// Parent is waiting for child to finish
			//
			Current->status = BLOCKED_ON_JOIN;

			//
			// call dispatcher
			//
			dispatcher();
		}	else {
			//
			// Child quit before parent called join()
			// therefore the child is on the parent's
			// child kill list
			//
			killedChild = dequeue(&Current->child_kill_list, FALSE);

			//
			// save child pid and exit code to
			// return after the child's space
			// is cleaned up
			//
			cpid = killedChild->pid;
			*code = killedChild->exit_code;

			//
			// Re-Initializing Child to Empty
			// Free Space from Previous Head
			//
			free((void*)killedChild->stack);
			bzero(killedChild, sizeof(proc_struct));
			killedChild->status = EMPTY;

			//
			// break out of inifinite loop
			//
			break;
		}
	}

	//
	// Process was zapped when being blocked by join
	//
	if (is_zapped()) {
		return -1;
	}

	//
	// return child's process identifer
	//
	return cpid;

} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code)
{
	proc_ptr zapping_proc_ptr;
	proc_ptr itr;
	int  returnVal;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("quit(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
	// check for active children, if found, halt
	//
	itr = Current->child_proc_ptr;
	if (itr != NULL) {
		while(1) {
			if (itr->status != QUIT){
				printf("quit(): process %d quit with active children. Halting...\n",Current->pid);
				halt(1);
			}
			if (itr->next_sibling_ptr==NULL){
				break;
			}
			itr=itr->next_sibling_ptr;
		}
	}


	//
	// update current's parent's information
	// to reflect current's (child) quitting
	//
	if (Current->parent_proc_ptr!=NULL){
		//
		// adding current (child) to Parent's Kill List
		//
		enqueue(&Current->parent_proc_ptr->child_kill_list,Current);

		//
		// removing current (child) from parent's child list
		//
		remove_child(&Current->parent_proc_ptr, Current);

		//
		// removing current from the ready list
		//
		returnVal = remove_from_list(&ready_list[Current->priority],Current);

		// free up parent if its waiting on the child
		if (Current->parent_proc_ptr->status==BLOCKED_ON_JOIN){
			Current->parent_proc_ptr->status=READY;
		}
	}

	//
	// release all processes that
	// are blocked on zap... those
	// processes that are waiting
	// for current process to quit
	//
	zapping_proc_ptr=NULL;
	if (is_zapped()) {
		while (1) {
			zapping_proc_ptr = dequeue2(&Current->zapping_list,FALSE);
			if (zapping_proc_ptr==NULL){
				break;
			}
			//
			// setting the zapping process
			// to ready releases the block
			//
			zapping_proc_ptr->status=READY;
		}
	}

	//
	// free up all children that the
	// quitting parent has not joined with
	//
	while(1){
		itr=dequeue(&Current->child_kill_list, FALSE);
		if (itr==NULL){
			break;
		}
		free((void*)itr->stack);
		bzero(itr, sizeof(proc_struct));
		itr->status = EMPTY;
	}

	//
	// set current status and exit code
	//
	Current->status = QUIT;
	Current->exit_code = code;
	Current->next_proc_ptr = NULL;

	//
	// used for future phases
	//
	p1_quit(Current->pid);

	//
	// call dispatcher
	//
	dispatcher();

} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
	proc_ptr next_process;	/* used to find next process to run */
	proc_ptr prev=Current;  /* used to save current process */
	int priority;						/* used to iterate through ready list */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("dispatcher(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
	// Saving Current Process
	//
	if (Current != NULL && Current->status != QUIT) {
		//
		// end current process running time
		//
		track_proc_end();

		//
		// calculate total cpu time used
		// by the current process
		//
		Current->cpu_time +=end-start;

		if (Current->status==RUNNING){
			//
			// if the Current process was RUNNING,
			// set its state to READY before
			// adding it back to the ready list
			//
			Current->status=READY;
		}
		//
		// add current to the ready list
		//
		enqueue(&ready_list[Current->priority],Current);
	}

	//
	// Finding next process to run
	//
	for (priority=MAXPRIORITY; priority <= LOWEST_PRIORITY; priority++){
		//
		// dequeue a process if and only if it is
		// in the READY state
		//
		next_process = dequeue(&ready_list[priority], TRUE);
		if (next_process !=NULL){
			//
			// we found a READY process of highest
			// priority, now exit the loop
			//
			break;
		}
	}

	//
	// Set New Process to Current
	//
	Current=next_process;
	Current->status=RUNNING;

	//
	// init start time
	//
	if (Current->start_time == 0){
		Current->start_time=sys_clock();
	}

	//
	// keep track of the newly
	// started current process
	//
	track_proc_start();

	//
	// switch to new context
	//
	if (prev == NULL){
		context_switch(NULL, &Current->state);
	} else {
		context_switch(&prev->state, &Current->state);
	}

	//
	// call p1_switch for future phases
	//
  p1_switch(Current->pid, next_process->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char * dummy)
{
	//
 	// test if in kernel mode; halt if in user mode
 	//
	if (! confirm_kernelmode()){
		printf("sentinel(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
	// detect and report deadlock state
	// or exit gracefully
	//
  while (1){
     waitint();
     check_deadlock();
  }
} /* sentinel */


/*
 * check to determine if deadlock has occurred...
 */
static void check_deadlock(void)
{
	int num_proc;	/* number of processes that exist */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("check_deadlock(): called while in user mode, by process %d. Halting... \n", Current->pid);
		halt(1);
	}

	//
	// cleanup will free up initial process,
	// which has no parent
	//
	cleanup();

	//
	// the sentinel should be the only process left
	//
	num_proc=nproc();
	if (num_proc > 1){
		printf("check_deadlock(): num_proc = %d\n", num_proc);
		printf("check_deadlock(): processes still present.  Halting...\n");
		halt(1);
	}

	printf("All processes completed.\n");
	halt(0);

} /* check_deadlock */


/*
 * Enable the interrupts.
 */
static void enableInterrupts(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("enableInterrupts(): called while in user mode. Halting... \n");
		halt(1);
	}

	//
	// turn the interrupts ON
	//
	psr_set(psr_get() | PSR_CURRENT_INT);

} /* enableInterrupts */


/*
 * Disables the interrupts.
 */
static void disableInterrupts(void)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("disableInterrupts(): called while in user mode. Halting... \n");
		halt(1);
	}

  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else {
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
	}

} /* disableInterrupts */


/* ------------------------------------------------------------------------
   Name - confirm_kernelmode
   Purpose - The purpose of the confirm_kernelmode is to make sure that
   	   the current mode is kernel mode
   Parameters - none
   Returns - 1 if the current mode is kernelmode
             0 otherwise
   Side Effects -  none (hopefully)
   ----------------------------------------------------------------------- */
int confirm_kernelmode(void)
{
	return PSR_CURRENT_MODE & psr_get();

} /* confirm_kernelmode */


/*
 * outputs specific processes from process table
 *
 * if the flag is set, all process slots are printed out
 * otherwise all non-empty process slots are printed out
 */
void dump_spec_procs(int flag){
	int i;			/* used to iterate through process table */
	char* str;	/* used to store status as a string */

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	//
	// print header
	//
	printf("PID\tParent\tPriority\tStatus\t\t# Kids\tCPUtime\tName\n");

	//
	// iterate through the process table
	//
	for (i=0; i < MAXPROC; i++){
		if (flag==FALSE && ProcTable[i].status==EMPTY){
			//
			// empty process slot, ignore if flag is not set
			//
			continue;
		}
		if (ProcTable[i].pid==0){
			printf(" -1\t");
		} else {
			printf(" %d\t", ProcTable[i].pid);
		}
		if (ProcTable[i].parent_proc_ptr != NULL){
			printf("  %d\t", ProcTable[i].parent_proc_ptr->pid);
		} else {

			if (ProcTable[i].status != EMPTY){
				printf("  -2\t");
			} else {
				printf("  -1\t");
			}
		}
		if (ProcTable[i].priority==0){
			printf("   -1\t\t");
		} else {
			printf("   %d\t\t", ProcTable[i].priority);
		}
		str=str_status(ProcTable[i].status);
		if (str != NULL){
			printf("%s\t", str);
			free((void*)str);
		} else {
			printf("%d\t\t", ProcTable[i].status);
		}
		if (ProcTable[i].status != BLOCKED_ON_JOIN
		&&  ProcTable[i].status != BLOCKED_ON_ZAP
		&&  ProcTable[i].status < 10){
			printf("\t");
		}
		printf("  %d\t", ProcTable[i].nchild);
		if (ProcTable[i].cpu_time==0){
			printf("   -1\t");
		} else {
			printf("   %d\t", ProcTable[i].cpu_time);
		}
		printf("%s\n", ProcTable[i].name);
	}
} /* dump_spec_procs */


/* ------------------------------------------------------------------------
	Name - dump_processes
	Purpose - The purpose of dump_processes is to print out all the process
						information in order to make debugging easier
	Paremeters - none
	Returns - nothing
	Side Effects - none (hopefully)
   ------------------------------------------------------------------------ */
void dump_processes(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	dump_spec_procs(TRUE);
} /* dump_proccesses */


/* ------------------------------------------------------------------------
	Name - dump_ready_list
	Purpose - The purpose of this function is to print out the ready list
						table to assist debugging
	Paremeters - none
	Returns - nothing
	Side Effects - none (hopefully)
	 ------------------------------------------------------------------------ */
void dump_ready_list(void){
	int priority;
	proc_ptr itr;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	printf("#############################################\n");
	printf("#\n");
	printf("# BEGIN READY LIST DUMP\n");
	printf("\n");
	if (Current==NULL){
		printf("There is no current process!!!\n\n\n");
	} else {
		printf("Current pid=%d\n\n\n", Current->pid);
	}
	for (priority=MAXPRIORITY; priority <= LOWEST_PRIORITY; priority++){
		printf("priority %d processes:\t", priority);
		if (ready_list[priority] != NULL){
			itr=ready_list[priority];
			while(1){
				printf("%d", itr->pid);
				if (itr->next_proc_ptr != NULL){
					printf(", ");
					itr=itr->next_proc_ptr;
				} else {
					break;
				}
			}
		}
		printf("\n");
	}

	printf("\n");
	printf("# END READY LIST DUMP\n");
	printf("#\n");
	printf("#############################################\n");
} /* dump_ready_list */


/*
 * mapping from integer to string representation
 * of status
 */
char* str_status(int status){
	char* str=(char*)malloc(MAX_STATUS_LEN);

	switch(status){
		case EMPTY:
			strcpy(str, "EMPTY");
			break;

		case READY:
			strcpy(str, "READY");
			break;

		case RUNNING:
			strcpy(str, "RUNNING");
			break;

		case BLOCKED_ON_JOIN:
			strcpy(str, "JOIN_BLOCK");
			break;

		case BLOCKED_ON_ZAP:
			strcpy(str, "ZAP_BLOCK");
			break;


		case QUIT:
			strcpy(str, "QUIT");
			break;

		case ZAPPED:
			strcpy(str, "ZAPPED");
			break;

		default:
			free((void*)str);
			str=NULL;
			break;
	}

	return str;
} /* str_status */


/*
 * returns process identifier
 * of the current process
 */
int getpid(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	return Current->pid;
} /* getpid */


/*
 * zaps process with given pid
 */
int zap(int pid){
	proc_ptr zapped_proc_ptr = NULL;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}


	if (Current->pid == pid ) {
		printf("zap(): process %d tried to zap itself.  Halting...\n",pid);
		halt(1);
	}



	zapped_proc_ptr = get_process_ptr(pid);

	if (zapped_proc_ptr == NULL){
		printf("zap(): process id: %d does not exists. Halting... \n",pid);
		halt(1);
	}

	if (zapped_proc_ptr->status != QUIT ) {

		zapped_proc_ptr->isZapped = TRUE;
		Current->status=BLOCKED_ON_ZAP;
		enqueue2(&zapped_proc_ptr->zapping_list,Current);
		dispatcher();

	}


	// The process is being Zapped while it's BLOCKED ON ZAP
	if (is_zapped())
		return -1;

	return 0;
} /* zap */


/*
 * returns 1 if the process is zapped
 * returns 0 otherwise
 */
int is_zapped(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	return Current->isZapped;

} /* is_zapped */


/*
 * returns a pointer to the process
 * with given pid
 */
proc_ptr get_process_ptr(int pid){
	int i;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	for (i=0; i < MAXPROC; i++){
		if (ProcTable[i].status != EMPTY){
			if (ProcTable[i].pid == pid)
				return &ProcTable[i];
		}
	}

	return NULL;
} /* get_process_ptr */


/*
 * returns the number of active processes
 */
int nproc(void){
	int n=0;
	int i;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	for (i=0; i < MAXPROC; i++){
		if ((ProcTable[i].status != EMPTY)
		&&  (ProcTable[i].status != QUIT )){
			n++;
		}
	}
	return n;
}

int block_me(int new_status) {
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}


	if (new_status <= 10 ) {
		printf("block_me(): new status %d is less or equal to 10. Halting... \n",new_status);
		halt(1);

	}


	Current->status = new_status;
	dispatcher();

	// The process is being Zapped while it's BLOCKED
	if (is_zapped())
		return -1;

	return 0;

} /* nproc */


/*
 * unblocks the process with
 * given process id
 */
int unblock_proc(int pid) {

	proc_ptr  proc_to_unblock =NULL;

	proc_to_unblock = get_process_ptr(pid);

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}


	if (proc_to_unblock==NULL)  // If it Does not exists
		return -2;

	if (Current->pid == pid)  // If it's the current process
		return -2;

	if (proc_to_unblock->status <= 10 )  // If it hasn't been blocked by block_me()
		return -2;


	proc_to_unblock->status =READY;

	dispatcher();

	// The process is being Zapped while it's BLOCKED
	if (is_zapped())
		return -1;

	return 0;

} /* unblock_proc */


/*
 * interrupt handler,
 * handles those rude interuptions
 */
void int_handler(int int_dev, int unit){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	switch(int_dev){
		case CLOCK_DEV:
				time_slice();
			break;
		default:
			printf("Unknown device called an interrupt\n");
			exit(1);
	}
} /* int_handler */


/*
 * returns cpu time used by the current
 * process in milliseconds
 */
int readtime(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	//
	// return the cpu time divided by
	// 1000 to convert cpu_time from
	// microseconds to milliseconds
	//
	return Current->cpu_time / 1000;
} /* readtime */


/*
 * returns the start time of the
 * process in microseconds since
 * the kernel first started
 */
int read_cur_start_time(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	return Current->start_time;
} /* read_cur_start_time */


/*
 * set global "start" to the
 * current sys_clock time
 */
void track_proc_start(){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	start=sys_clock();
} /* track_proc_start */

/*
 * set global "end" to the
 * current sys_clock time
 */
void track_proc_end(){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	end=sys_clock();
} /* track_proc_end */


/*
 * returns a stack size which is
 * a multiple of USLOSS_MIN_STACK
 * rounded up from given memsize
 */
int stackallign(int memsize){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	int newsize=USLOSS_MIN_STACK;
	if (memsize % USLOSS_MIN_STACK != 0){
		while(newsize < memsize){
			newsize+=USLOSS_MIN_STACK;
		}
	} else {
		newsize=memsize;
	}
		return newsize;
} /* stackallign */


/*
 * initializes the process
 * in the process table
 */
static void init_proc(int index){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	bzero(&ProcTable[index], sizeof(proc_struct));
	ProcTable[index].status=EMPTY;
} /* init_proc */


/*
 * initializes all processes
 * who have quit, but haven't
 * been cleaned up
 */
void cleanup(void){
	int i;

	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	for (i=0; i < MAXPROC; i++){
		if (ProcTable[i].status == QUIT){
			free((void*)ProcTable[i].stack);
			init_proc(i);
		}
	}
} /* cleaup */


/*
 * calls the dispatcher if the
 * current process has exceeded
 * its time slice
 */
void time_slice(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	if (! confirm_kernelmode()){
		printf("Error: Not in kernel mode.");
		halt(1);
	}

	if (sys_clock()-start > 80000)
		dispatcher();
} /* time_slice */
