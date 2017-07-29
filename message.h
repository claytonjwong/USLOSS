
#ifndef _MESSAGE_H
#define _MESSAGE_H

//////////////////////////////////////////////////////////////
// definitions
//
#define DEBUG2 1
#define BLOCKED_ON_SEND 11
#define BLOCKED_ON_RECV	12
#define BLOCKED_ON_RELEASE 13
#define MESG_ALREADY_RECV 14
#define MESG_ALREADY_SENT 15
//
#define CLOCK_MBOX_ID		0
#define	TERM1_MBOX_ID		1
#define	TERM2_MBOX_ID		2
#define	TERM3_MBOX_ID		3
#define	TERM4_MBOX_ID		4
#define DISK1_MBOX_ID		5
#define DISK2_MBOX_ID		6
//
#define SEND	1
#define RECV  2
//
enum {EMPTY, USED, RELEASED};
enum {FALSE, TRUE};
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// structures
//
typedef struct mail_slot * mail_slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc * mbox_proc_ptr;
typedef struct pidList * list_ptr;
typedef struct proc_struct proc_struct;
typedef struct proc_struct * proc_ptr;

struct proc_struct {
	short  pid;
	int    status;
	void*	mesg;
	int mesg_sz;

   /* other fields as needed... */
};

struct mailbox {
   int           mbox_id;
   mail_slot_ptr mesg_list;
   list_ptr		 send_wait_list;
   list_ptr		 recv_wait_list;
   int					blocked_release_proc;
   int					release_count;
   int           status;
   int			 mesg_count;
   int           num_slot;
   int			 max_mesg_sz;
   /* other items as needed... */
};


struct pidList {
   list_ptr     next;
   int          pid;
};


struct mail_slot {
   mail_slot_ptr   next;
   int      	   mbox_id;
   int      	   status;
   void  *  	   mesg;
   int      	   mesg_sz;
   /* other items as needed... */
};

struct psr_bits {
   unsigned int unused:28;
   unsigned int prev_int_enable:1;
   unsigned int prev_mode:1;
   unsigned int cur_int_enable:1;
   unsigned int cur_mode:1;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// function declarations
//
int MboxCreate(int slots, int slot_size);
int MboxRelease(int mbox_id);
int MboxSend(int mbox_id, void *mesg, int mesg_sz);
int MboxReceive(int mbox_id, void *buf, int buf_sz);
int MboxCondSend(int mbox_id, void* mesg, int mesg_sz);
int MboxCondReceive(int mbox_id, void *buf, int buf_sz);
int check_io(void);
int waitdevice(int type, int unit, int* status);
//
//////////////////////////////////////////////////////////////

#endif
