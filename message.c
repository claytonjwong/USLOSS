//////////////////////////////////////////////////////
//
// Programmers: Boris Salazar and Clayton Wong
//
// message.c
//
// description: used to send messages to/from processes
// in order to synchronize them
//
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
// header files
//
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <message.h>
#include <handler.h>
#include <queue.h>
//
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
// prototypes
//
int start1 (char *);
int start2 (char *);
//
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
// globals
//
int debugflag2 = 0;

//
// the mail boxes, used to hold messages
//
static mail_box MailBoxTable[MAXMBOX];

//
// the process table
//
static proc_struct ProcTable[MAXPROC];

//
// the total slot count
//
int total_slot=0;

//
// sys_vec is used in later phases
// for system calls
//
void (*sys_vec[MAXSYSCALLS])(sysargs *args);

//
// waiting for i/o flag
//
int io_wait = FALSE;
//
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
// function declarations
//
static void initMailBox(int mbox_id);
static int find_proc(int pid);
static void init_proc(int index);
static int store_proc(int pid, void* mesg, int mesg_sz, int action);
static void check_kernel_mode(char* func_name);
static void disableInterrupts(void);
static void enableInterrupts(void);
static int sendmsg(int mbox_id, void* mesg, int mesg_sz, int wait);
static int recvmsg(int mbox_id, void* buf, int buf_sz, int wait);
static int is_valid_params(int mbox_id, void* mesg, int mesg_sz, int action);
//
//////////////////////////////////////////////////////


/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
	int kid_pid;
	int status;
	int i;

   if (DEBUG2 && debugflag2)
      console("start1(): at beginning\n");

   check_kernel_mode("start1");

   /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

	//
	// initialize mailboxes
	//
	for ( i=0; i <MAXMBOX; i++){
		initMailBox(i);
	}

	//
	// I/O mailboxes have one slot, and
	// a message size big enough to store
	// the status of what waitdevice returns
	//

	//
	// create mailbox for clock
	//
	if (MboxCreate(1, sizeof(int)) != CLOCK_MBOX_ID){
		fprintf(stderr, "Internal Error: creating clock i/o mailbox\n");
		exit(1);
	}

	//
	// create mailbox for terminals
	//
	if (MboxCreate(1, sizeof(int)) != TERM1_MBOX_ID){
		fprintf(stderr, "Internal Error: creating term1 i/o mailbox\n");
		exit(1);
	}
	if (MboxCreate(1, sizeof(int)) != TERM2_MBOX_ID){
		fprintf(stderr, "Internal Error: creating term2 i/o mailbox\n");
		exit(1);
	}
	if (MboxCreate(1, sizeof(int)) != TERM3_MBOX_ID){
		fprintf(stderr, "Internal Error: creating term3 i/o mailbox\n");
		exit(1);
	}
	if (MboxCreate(1, sizeof(int)) != TERM4_MBOX_ID){
		fprintf(stderr, "Internal Error: creating term4 i/o mailbox\n");
		exit(1);
	}

	//
	// create mailbox for disks
	//
	if (MboxCreate(1, sizeof(int)) != DISK1_MBOX_ID){
		fprintf(stderr, "Internal Error: creating disk1 i/o mailbox\n");
		exit(1);
	}
	if (MboxCreate(1, sizeof(int)) != DISK2_MBOX_ID){
		fprintf(stderr, "Internal Error: creating disk2 i/o mailbox\n");
		exit(1);
	}

	//
	// initialize proc table
	//
	for (i=0; i < MAXPROC; i++){
		init_proc(i);
	}

	//
	// initialize int_vec
	//
	int_vec[CLOCK_DEV]=clock_handler;
	int_vec[ALARM_DEV]=alarm_handler;
	int_vec[DISK_DEV]=disk_handler;
	int_vec[TERM_DEV]=term_handler;
	int_vec[MMU_INT]=mmu_handler;
	int_vec[SYS_INT]=syscall_handler;

	//
	// initialize sys_vec
	//
	for (i=0; i < MAXSYSCALLS; i++){
		sys_vec[i]=nullsys;
	}

	/* Create a process for start2, then block on a join until start2 quits */
	if (DEBUG2 && debugflag2)
		console("start1(): fork'ing start2 process\n");
	kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);

	int temp_pid	= join(&status);
	if (  temp_pid!= kid_pid ) {
		//printf(" Join Return Value: %d  Status: %d   Kid Id: %d \n",temp_pid, status, kid_pid);
		console("start2(): join returned something other than start2's pid\n");
	}

	quit(0);
  return 0;
} /* start1 */


//////////////////////////////////////////////////////
//
// initMailBox
//
// description: initializes a mailbox by zeroing it out
// then setting its mbox_id to its index in the table
// and setting its status to EMPTY
//
static void initMailBox(int mbox_id) {
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("initMailBox");

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	bzero(&MailBoxTable[mbox_id], sizeof(mail_box));
	MailBoxTable[mbox_id].mbox_id = mbox_id;
	MailBoxTable[mbox_id].status = EMPTY;

	enableInterrupts();
	//
	// end critial section
	//////////////////////
}
//
// end initMailBox function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// init_proc
//
// description: initializes the process in the
// process table by zeroing it out, and setting
// its status to EMPTY
//
static void init_proc(int index){
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("init_proc");

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	bzero(&ProcTable[index], sizeof(proc_struct));
	ProcTable[index].status=EMPTY;

	enableInterrupts();
	// end critial section
	//////////////////////
}
//
// end init_proc function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// store_proc
//
// description: stores the process with param "pid" into
// an empty slot in the process table by finding an
// empty slot in the process table, setting its status to
// USED, then storing the processes pid, message, and
// message size there if the action is SEND, if the action
// is RECV, then the message pointer is stored, rather
// than the message itself
//
// returns the index in the process table where the
// process is stored
// OR
// returns -1 if there is not room to store the process
// in the process table
//
static int store_proc(int pid, void* mesg, int mesg_sz, int action){
	int proc_slot;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("store_proc");

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	//
	// find an empty slot in the process table
	//
	proc_slot=0;
	do{
		if (ProcTable[proc_slot].status==EMPTY){
			break;
		}
		proc_slot++;
	} while (proc_slot < MAXPROC);

	//
	// if proc_slot is equal to MAXPROC, then
	// we did not find an empty slot from 0
	// to MAXPROC -1 inclusive, therefore
	// there is not an empty slot in the
	// process table
	//
	if (proc_slot==MAXPROC){
		return -1;
	}

	ProcTable[proc_slot].status = USED;

	enableInterrupts();
	//
	// end critial section
	//////////////////////

	//
	// save information
	//
	ProcTable[proc_slot].pid = pid;

	if (action==SEND) {
		ProcTable[proc_slot].mesg = malloc(mesg_sz);
		memcpy(ProcTable[proc_slot].mesg, mesg,mesg_sz );
	}
	else {
		ProcTable[proc_slot].mesg = mesg;
	}

	ProcTable[proc_slot].mesg_sz = mesg_sz;

	return proc_slot;
}
//
// end store_proc function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// find_proc
//
// description: searches the process table for a
// process with "pid"
//
// returns the index of the process in the process
// table if it is found
// OR
// returns -1 if the process was not found
//
static int find_proc(int pid){
	int proc_slot;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("find_proc");

	//
	// search the process table
	//
	proc_slot=0;
	do{
		if (ProcTable[proc_slot].pid==pid){
			//
			// process found
			//
			return proc_slot;
		}
		proc_slot++;
	} while (proc_slot < MAXPROC);

	//
	// process not found
	//
	return -1;
}
//
// end find_proc function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// dispose_proc
//
// description: removes the process with "pid"
// from the process table
//
// returns 0 if the process was sucessfully removed
// OR
// returns -1 if the process was not found
//
static int dispose_proc(int pid){
	int proc_slot;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("dispose_proc");

	//
	// find the process index
	// in the process table
	//
	proc_slot = find_proc(pid);

	if (proc_slot < 0){
		return -1;
	}

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	free((void*)ProcTable[proc_slot].mesg);
	bzero(&ProcTable[proc_slot], sizeof(proc_struct));
	ProcTable[proc_slot].status = EMPTY;

	enableInterrupts();
	//
	// end critial section
	//////////////////////

	return 0;
}
//
// end dispose_proc function
//////////////////////////////////////////////////////


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{

	int i;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxCreate");

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	for ( i=0; i<MAXMBOX; i++ ){
		if (MailBoxTable[i].status ==EMPTY) {
			break;
		}
	}

	if ( i == MAXMBOX ) {
		return -1;
	}

	MailBoxTable[i].status = USED;
	MailBoxTable[i].num_slot = slots;
	MailBoxTable[i].max_mesg_sz = slot_size;

	enableInterrupts();
	//
	// end critial section
	//////////////////////

	return i;
}
//
// end MboxCreate function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// MboxRelease
//
// description: re-initializes a mailbox, by freeing up all
// processes blocked on the mailbox, then re-initializes
// the mailbox
//
int MboxRelease(int mbox_id) {

	list_ptr release_proc;
	int curr_proc_slot;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxRelease");

	//
	// make sure the mailbox is non-empty
	//
	if (MailBoxTable[mbox_id].status==EMPTY)
		return -1;

	//
	// set mailbox status to RELEASED
	//
	MailBoxTable[mbox_id].status = RELEASED;

	//
	// release processes blocked on send
	// and keep track of the number of processes
	// in order to tell which process is
	// the last process unblocked
	//
	MailBoxTable[mbox_id].release_count = 0;
	while(1){
		//
		// release send wait list
		//
		release_proc=remove_proc(&MailBoxTable[mbox_id].send_wait_list);
		if (release_proc == NULL){
			break;
		} else {
			//
			// increment release count for this mailbox
			//
			MailBoxTable[mbox_id].release_count++;
			//
			// unblock process on send wait list
			//
			unblock_proc(release_proc->pid);
		}
	}

	//
	// release processes blocked on receive
	// and keep track of the number of processes
	// in order to tell which process is
	// the last process unblocked
	//
	while(1){
		//
		// release recv wait list
		//
		release_proc=remove_proc(&MailBoxTable[mbox_id].recv_wait_list);
		if (release_proc == NULL){
			break;
		} else {
			//
			// increment release count for this mailbox
			//
			MailBoxTable[mbox_id].release_count++;
			//
			// unblock process on recv wait list
			//
			unblock_proc(release_proc->pid);
		}
	}

	//
	// if the release count is zero, then all
	// processes have been released, otherwise
	// block until they have been released,
	// the last unblocked process should then
	// free up the current process
	//
	if (MailBoxTable[mbox_id].release_count > 0){
		curr_proc_slot = find_proc(getpid());
		ProcTable[curr_proc_slot].status = BLOCKED_ON_RELEASE;
		MailBoxTable[mbox_id].blocked_release_proc = getpid();
		block_me(BLOCKED_ON_RELEASE);
	}
	//
	// set status back to in use
	//
	ProcTable[curr_proc_slot].status = USED;
	//
	// re-initialize the mailbox
	//
	initMailBox(mbox_id);

	//
	// check to see if the current process
	// was zapped
	//
	if (is_zapped())
		return -3;

	return 0;
}
//
// end MboxRelease function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// is_valid_params
//
// description: makes sure that the given parameters
// are valid for mailbox usage
//
// returns 0 if the parameters are valid
// OR
// returns -1 if the parameters are not valid
//
static int is_valid_params(int mbox_id, void* mesg, int mesg_sz, int action){

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("is_valid_params");

	//
	// make sure the mailbox exists
	//
	if (MailBoxTable[mbox_id].status==EMPTY){
		return -1;
	}

	//
	// make sure the mailbox is not released
	//
	if (MailBoxTable[mbox_id].status==RELEASED){
		return -1;
	}

	//
	// If message size in either case is <0
	//
	if (mesg_sz < 0){
		return -1;
	}

	//
	// Make sure that the mesg_sz <= max_mesg_sz
	// of the mailbox
	//
	if (action==SEND && mesg != NULL){
		if (mesg_sz > MailBoxTable[mbox_id].max_mesg_sz){
			return -1;
		}
	}

	//
	// parameters are valid
	//
	return 0;
}
//
// end is_valid_params function
//////////////////////////////////////////////////////


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *mesg, int mesg_sz)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxSend");

	return sendmsg(mbox_id, mesg, mesg_sz, TRUE);
}
//
// end MboxSend function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// MboxCondSend
//
// description: put a message into a slot for the indicated
// mailbox, however, if there is not an empty slot in the
// mailbox, do not block, instead, return -2
//
int MboxCondSend(int mbox_id, void* mesg, int mesg_sz)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxCondSend");

	return sendmsg(mbox_id, mesg, mesg_sz, FALSE);
}
//
// end MboxCondSend function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// sendmsg
//
// description: sends the message pointed to my mesg
// to the mailbox with mbox_id, if the wait flag is
// set, then this function will block until there
// is an empty slot in the message box to send to
//
// if the wait flags is not set, then this function
// will not block if there is not an empty slot
// in the message box to send to, instead it will
// return -2
//
// returns 0 upon sucessful completion
// OR
// returns -1 if any of the parameters are invalid
// OR
// returns -2 if the wait flag was not set and there
// was not room in the mailbox for the message
//
static int sendmsg(int mbox_id, void* mesg, int mesg_sz, int wait){

	int rc;
	int proc_slot;
	int unblock_slot;
	list_ptr wait_recv_proc;
	mail_slot_ptr new_mesg =NULL;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("sendmsg");

	//
	// check for valid parameters
	//
	if (is_valid_params(mbox_id, mesg, mesg_sz,SEND) < 0){
		return -1;
	}

	//
	// store information into proc_table
	// about the currently sending process
	// and get the slot it is stored in the
	// process table
	//
	proc_slot = store_proc(getpid(), mesg, mesg_sz, SEND);

	///////////////////////////////////////////////
	//
	// special case: mailbox has no slots
	//
	if (MailBoxTable[mbox_id].num_slot ==0) {
		//
		// see if there is a process blocked on receive
		//
		wait_recv_proc = remove_proc(&MailBoxTable[mbox_id].recv_wait_list);

		if (wait_recv_proc != NULL) {
			//
			// copy message directly to the process
			// that is blocked on receive
			//
			unblock_slot = find_proc(wait_recv_proc->pid);
			memcpy(ProcTable[unblock_slot].mesg, mesg, mesg_sz);
			ProcTable[unblock_slot].mesg_sz = mesg_sz;
			//
			// free up a process waiting to
			// receive on this mailbox
			//
			unblock_proc(wait_recv_proc->pid);
		} else {
			//
			// save the message directly to the
			// send process
			//
			memcpy(ProcTable[proc_slot].mesg, mesg, mesg_sz);
			ProcTable[proc_slot].mesg_sz = mesg_sz;

			//
			// there was not a process blocked on receive, add
			// myself to the send wait list of this mailbox
			//
			add_proc(&MailBoxTable[mbox_id].send_wait_list, getpid());
			//
			// set process status to BLOCKED_ON_SEND
			//
			ProcTable[proc_slot].status = BLOCKED_ON_SEND;
			//
			// block the current process
			//
			rc = block_me(BLOCKED_ON_SEND);

			//
			// check to see if the mailbox has been released
			//
			if (MailBoxTable[mbox_id].status==RELEASED){
				//
				// decrement the release count
				//
				MailBoxTable[mbox_id].release_count--;

				//
				// check to see if we were the last process released
				//
				if (MailBoxTable[mbox_id].release_count == 0){
					//
					// if so, unblock the process blocked on release
					// of this mailbox
					//
					unblock_proc(MailBoxTable[mbox_id].blocked_release_proc);
				}
				return -3;
			}

			if (rc < 0){
				return -3;
			}
		}

		if (is_zapped()){
			return -3;
		}

		//
		// now that the message has been sent
		// dispose of the process that sent it
		//
		dispose_proc(getpid());

		return 0;
	}
	//
	// end special case
	//
	///////////////////////////////////////////////



	//
	// wait for an empty slot if flag is
	// set, otherwise return immediately
	//
	if (wait==TRUE){
		//
		// make sure there is space in the mailbox
		//
		if (MailBoxTable[mbox_id].mesg_count >= MailBoxTable[mbox_id].num_slot){

			//////////////////////
			// begin critial section
			//
			disableInterrupts();

			//
			// place current process onto the mailbox's
			// send wait list
			//
			add_proc(&MailBoxTable[mbox_id].send_wait_list, getpid());

			//
			// set process status to BLOCKED_ON_SEND
			//
			ProcTable[proc_slot].status = BLOCKED_ON_SEND;

			enableInterrupts();
			//
			// end critial section
			//////////////////////

			//
			// block current process
			//
			rc = block_me(BLOCKED_ON_SEND);
		}
	} else {
		//
		// make sure there is space in the mailbox
		//
		if (MailBoxTable[mbox_id].mesg_count >= MailBoxTable[mbox_id].num_slot){
			return -2;
		}
	}

	//
	// At this point there should be an empty mailbox slot for either case
	// or the process got unblocked because the mailbox was released
	//

	//
	// check to see if the mailbox has been released
	//
	if (MailBoxTable[mbox_id].status==RELEASED){
		//
		// decrement the release count
		//
		MailBoxTable[mbox_id].release_count--;

		//
		// check to see if we were the last process released
		//
		if (MailBoxTable[mbox_id].release_count == 0){
			//
			// if so, unblock the process blocked on release
			// of this mailbox
			//
			unblock_proc(MailBoxTable[mbox_id].blocked_release_proc);
		}
		return -3;
	}

	if (ProcTable[proc_slot].status == MESG_ALREADY_SENT) {
		//
		// now that the message has been sent
		// dispose of the process that sent it
		//
		dispose_proc(getpid());

		//
		// check to see if current
		// process was zapped
		//
		if (is_zapped() || rc < 0)
			return -3;


		return 0;

	}

	//
	// free up a process waiting to
	// receive on this mailbox
	//
	wait_recv_proc = remove_proc(&MailBoxTable[mbox_id].recv_wait_list);
	if (wait_recv_proc != NULL){
		//
		// then directly copy message
		// don't use the mailbox
		//
		unblock_slot = find_proc(wait_recv_proc->pid);
		memcpy(ProcTable[unblock_slot].mesg, mesg, mesg_sz);
		ProcTable[unblock_slot].mesg_sz = mesg_sz;
		ProcTable[unblock_slot].status = MESG_ALREADY_RECV;
		unblock_proc(wait_recv_proc->pid);
	} else {
		//
		// create and initialize new message
		// and stick it into the mailbox
		//
		new_mesg = malloc(sizeof(struct mail_slot));
		bzero(new_mesg,sizeof(struct mail_slot));
		new_mesg->mbox_id = mbox_id;
		new_mesg->mesg = malloc(mesg_sz);
		memcpy(new_mesg->mesg, ProcTable[proc_slot].mesg, mesg_sz);
		new_mesg->mesg_sz = mesg_sz;

		//////////////////////
		// begin critial section
		//
		disableInterrupts();

		//
		// make sure there are enough
		// slots total to send another
		// message
		//
		if (total_slot >= MAXSLOTS){
			//
			// there is not enough room
			//
			dispose_proc(getpid());
			return -2;
		}

		//
		// put new message into the mailbox
		//
		putmesg(&MailBoxTable[mbox_id].mesg_list,new_mesg);

		//
		// increment mailbox message count
		//
		MailBoxTable[mbox_id].mesg_count++;

		//
		// increment the total amount of slots
		//
		total_slot++;

		enableInterrupts();
		//
		// end critial section
		//////////////////////
	}

	//
	// now that the message has been sent
	// dispose of the process that sent it
	//
	dispose_proc(getpid());

	//
	// check to see if current
	// process was zapped
	//
	if (is_zapped() || rc < 0)
		return -3;


	return 0;
}


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *buf, int buf_sz)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxReceive");

	return recvmsg(mbox_id, buf, buf_sz, TRUE);
}
//
// end MboxReceive function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// MboxCondReceive
//
// description: attempts to receive a message
// if there are no mesages in the mailbox with mbox_id
// then no message will be received since this function
// is non-blocking
//
// returns the size of the message received
// OR
// returns -2 if there is no message in the mailbox to receive
//
int MboxCondReceive(int mbox_id, void *buf, int buf_sz)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("MboxCondReceive");

	return recvmsg(mbox_id, buf, buf_sz, FALSE);
}
//
// end MboxCondReceive function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// recvmsg
//
// description: attempts to receive a message from the
// mailbox with mbox_id.  If the wait flag is set then
// this function will block until there is a message
// available for it to receive
//
// if the wait flag is not send, and there is not a message
// in the message box with mbox_id then this function
// will return -2 and not block
//
// returns the size of the message received
// OR
// returns -2 if there is not a message in the mailbox
// to receive and the wait flag is not set
//
static int recvmsg(int mbox_id, void* buf, int buf_sz, int wait){
	int rc;
	int proc_slot;
	int unblock_slot;
	int recvd_mesg_sz=0;
	list_ptr wait_send_proc;
	mail_slot_ptr recvd_mesg;
	mail_slot_ptr new_mesg;

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("recvmsg");

	//
	// check for valid parameters
	//
	if (is_valid_params(mbox_id, buf, buf_sz, RECV) < 0){
		return -1;
	}
	//
	// store information into proc_table
	// about the currently receiving process
	// and get the slot it is stored in the
	// process table
	//
	proc_slot = store_proc(getpid(), buf, buf_sz, RECV);

	///////////////////////////////////////////////
	//
	// special case: zero-slot mailbox
	//
	if (MailBoxTable[mbox_id].num_slot ==0) {
		//
		// see if there is a process blocked on receive
		//
		wait_send_proc = remove_proc(&MailBoxTable[mbox_id].send_wait_list);

		if (wait_send_proc != NULL) {
			//
			// receive the message of the process blocked on send
			// by directly copying it to buf
			//
			unblock_slot = find_proc(wait_send_proc->pid);
			memcpy(buf, ProcTable[unblock_slot].mesg, ProcTable[unblock_slot].mesg_sz);
			recvd_mesg_sz=ProcTable[unblock_slot].mesg_sz;
			//
			// free up a process waiting to
			// receive on this mailbox
			//
			unblock_proc(wait_send_proc->pid);
		} else {
			//
			// there was not a process blocked on receive, add
			// myself to the recv wait list of this mailbox
			//
			add_proc(&MailBoxTable[mbox_id].recv_wait_list, getpid());
			//
			// set process status to BLOCKED_ON_RECV
			//
			ProcTable[proc_slot].status = BLOCKED_ON_RECV;
			//
			// block the current process
			//
			rc = block_me(BLOCKED_ON_RECV);
			//
			// check to see if the mailbox has been released
			//
			if (MailBoxTable[mbox_id].status==RELEASED){
				//
				// decrement the release count
				//
				MailBoxTable[mbox_id].release_count--;

				//
				// check to see if we were the last process released
				//
				if (MailBoxTable[mbox_id].release_count == 0){
					//
					// if so, unblock the process blocked on release
					// of this mailbox
					//
					unblock_proc(MailBoxTable[mbox_id].blocked_release_proc);
				}
				return -3;
			}

			if (rc < 0){
				return -3;
			}

			recvd_mesg_sz = ProcTable[proc_slot].mesg_sz;
		}

		if (is_zapped()){
			return -3;
		}

		//
		// now that the message has been received
		// dispose of the process that sent it
		//
		dispose_proc(getpid());

		return recvd_mesg_sz;
	}
	//
	// end special case
	///////////////////////////////////////////////

	//
	// wait for something to be recv'd from the mailbox
	//
	if (wait==TRUE){
		//
		// make sure there is something to receive
		//
		if (MailBoxTable[mbox_id].mesg_count == 0){
			//
			// place current process onto the mailbox's
			// recv wait list
			//
			add_proc(&MailBoxTable[mbox_id].recv_wait_list, getpid());
			//
			// set process status to BLOCKED_ON_SEND
			//
			ProcTable[proc_slot].status = BLOCKED_ON_RECV;
			//
			// block current process
			//
			rc = block_me(BLOCKED_ON_RECV);
		}
	} else {
		//
		// make sure there is something to receive
		//
		if (MailBoxTable[mbox_id].mesg_count == 0){
			return -2;
		}
	}
	//
	// At this point there should be something in
	// the mailbox slot for either case
	// or the process got unblocked because the mailbox was released
	//

	//
	// check to see if the mailbox has been released
	//
	if (MailBoxTable[mbox_id].status==RELEASED){
		//
		// decrement the release count
		//
		MailBoxTable[mbox_id].release_count--;

		//
		// check to see if we were the last process released
		//
		if (MailBoxTable[mbox_id].release_count == 0){
			//
			// if so, unblock the process blocked on release
			// of this mailbox
			//
			unblock_proc(MailBoxTable[mbox_id].blocked_release_proc);
		}
		return -3;
	}

	if (ProcTable[proc_slot].status == MESG_ALREADY_RECV){
		//
		// check to see if current
		// process was zapped
		//
		if (is_zapped())
			return -3;

		//
		// check return code
		//
		if (rc < 0){
			//
			// then the process was zapped during
			// the block
			//
			return -3;
		}

		//
		// return message size received
		//
		return ProcTable[proc_slot].mesg_sz;
	}

	//////////////////////
	// begin critial section
	//
	disableInterrupts();

	//
	// receive a message from the mailbox
	//
	recvd_mesg = getmesg(&MailBoxTable[mbox_id].mesg_list);

	//
	// decrement mailbox message count
	//
	MailBoxTable[mbox_id].mesg_count--;

	//
	// decrement the total slot count
	//
	total_slot--;

	enableInterrupts();
	//
	// end critial section
	//////////////////////

	//
	// make sure buffer is big enough to hold
	// the message
	//
	if (recvd_mesg->mesg_sz > buf_sz){
		return -1;
	}

	//
	// save message to buffer
	//
	memcpy(buf, recvd_mesg->mesg, recvd_mesg->mesg_sz);

	//
	// remember message size to return later
	//
	recvd_mesg_sz = recvd_mesg->mesg_sz;

	//
	// free message
	//
	free((void*)recvd_mesg);

	//
	// free up a process waiting to
	// send on this mailbox
	//
	wait_send_proc = remove_proc(&MailBoxTable[mbox_id].send_wait_list);
	if (wait_send_proc != NULL){

		unblock_slot = find_proc(wait_send_proc->pid);
		//
		// create and initialize new message
		// and stick it into the mailbox
		//
		new_mesg = malloc(sizeof(struct mail_slot));
		bzero(new_mesg,sizeof(struct mail_slot));
		new_mesg->mbox_id = mbox_id;
		new_mesg->mesg = malloc(ProcTable[unblock_slot].mesg_sz);
		memcpy(new_mesg->mesg, ProcTable[unblock_slot].mesg, ProcTable[unblock_slot].mesg_sz);
		new_mesg->mesg_sz = ProcTable[unblock_slot].mesg_sz;

		//////////////////////
		// begin critial section
		//
		disableInterrupts();

		//
		// put new message into the mailbox
		//
		putmesg(&MailBoxTable[mbox_id].mesg_list,new_mesg);

		//
		// increment mailbox message count
		//
		MailBoxTable[mbox_id].mesg_count++;

		//
		// increment the total amount of slots
		//
		total_slot++;

		ProcTable[unblock_slot].status = MESG_ALREADY_SENT;

		enableInterrupts();
		//
		// end critial section
		//////////////////////

		unblock_proc(wait_send_proc->pid);

	}

	//
	// now that the message has been sent
	// dispose of the process that sent it
	//
	dispose_proc(getpid());

	//
	// check to see if current
	// process was zapped
	//
	if (is_zapped())
		return -3;

	//
	// check return code
	//
	if (rc < 0){
		//
		// then the process was zapped during
		// the block
		//
		return -3;
	}

	//
	// return message size received
	//
	return recvd_mesg_sz;
}
//
// end recvmsg function
//////////////////////////////////////////////////////


/* ------------------------------------------------------------------------
   Name - check_kernel_mode
   Purpose - The purpose of the confirm_kernelmode is to make sure that
   	   the current mode is kernel mode
   Parameters - none
   Returns - nothing
   Side Effects -  none (hopefully)
   ----------------------------------------------------------------------- */
static void check_kernel_mode(char* func_name)
{
	if(! PSR_CURRENT_MODE & psr_get()){
		printf("%s called while in user mode. Halting...\n", func_name);
		halt(1);
	}

}
//
// end check_kernel_mode function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// enableInterrupts
//
// description: enables interrupts
//
static void enableInterrupts(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("enableInterrupts");

	//
	// turn the interrupts ON
	//
	psr_set(psr_get() | PSR_CURRENT_INT);

}
//
// end enableInterrupts function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// disableInterrupts
//
// description: disables interrupts
//
static void disableInterrupts(void)
{
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("disableInterrupts");

  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else {
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
	}

}
//
// end disableInterrupts function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// check_io
//
// description: finds out if the current process is
// waiting on input or output by checking the global
// "io_wait" var
//
// returns 1 if the current process is waiting on I/O
// OR
// return 0 if the current process is not waiting on I/O
//
int check_io(void){
	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("check_io");

	return io_wait;
}
//
// end check_io function
//////////////////////////////////////////////////////


//////////////////////////////////////////////////////
//
// waitdevice
//
// description: waits for device input from a given
// device
//
// returns 0 upon sucessfully waiting for device input
// OR
// returns -1 if the device or device unit is unknown
//
int waitdevice(int type, int unit, int* status){

	//
	// test if in kernel mode; halt if in user mode
	//
	check_kernel_mode("waitdevice");

	//
	// set io_wait to TRUE, meaning that the current
	// process is waiting on input or output from a device
	//
	io_wait=TRUE;

	switch(type){
		//
		// clock
		//
		case CLOCK_DEV:
			MboxReceive(CLOCK_MBOX_ID, status, sizeof(int));
			break;

		//
		// terminal
		//
		case TERM_DEV:
			switch(unit){
				case 1:
					MboxReceive(TERM1_MBOX_ID, status, sizeof(int));
					break;
				case 2:
					MboxReceive(TERM2_MBOX_ID, status, sizeof(int));
					break;
				case 3:
					MboxReceive(TERM3_MBOX_ID, status, sizeof(int));
					break;
				case 4:
					MboxReceive(TERM4_MBOX_ID, status, sizeof(int));
					break;
				default:
					//
					// set io_wait flag to false,
					// since we are not waiting
					// on I/O anymore
					//
					io_wait=FALSE;
					return -1;
			}
			break;

		//
		// disk
		//
		case DISK_DEV:
			switch(unit){
				case 1:
					MboxReceive(DISK1_MBOX_ID, status, sizeof(int));
					break;
				case 2:
					MboxReceive(DISK2_MBOX_ID, status, sizeof(int));
					break;
				default:
					//
					// set io_wait flag to false,
					// since we are not waiting
					// on I/O anymore
					//
					io_wait=FALSE;
					return -1;
			}
			break;

		//
		// unknown device
		//
		default:
			//
			// set io_wait flag to false,
			// since we are not waiting
			// on I/O anymore
			//
			io_wait=FALSE;
			return -1;
	}

	//
	// set io_wait to FALSE, now that the input or output
	// for the current process has completed
	//
	io_wait=FALSE;

	return 0;
}
//
// end waitdevice function
//////////////////////////////////////////////////////
