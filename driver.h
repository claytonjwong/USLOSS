enum { FALSE, TRUE };

enum {EMPTY, INUSE, SLEEPING_BEAUTY, WAIT_ON_DISK};


#define N_BUF_LINE 10


typedef struct proc_struct * proc_ptr;

typedef struct diskreq_t* diskreq_ptr;

typedef struct diskreq_t {
	struct diskreq_t* next_req;
	int proc_slot;
	int track;
	int ndisk_track;
	int first;
	//int sectors;
	void* buf;
	char action;
} diskreq_t;


typedef struct termreq_t {
	int proc_slot;
	char action;
	int unit;
	//
	// buffer
	//
	int buf_sz;
	char* buf;


} termreq_t;



struct proc_struct {

	int 			pid;
	proc_ptr 	next;
	int 			status;
	int 			sleep_start_time;
	int 			sleep_time;
	//int 			private_mbox;
	int psem;
	diskreq_t disk_req;
	termreq_t term_req;
};


