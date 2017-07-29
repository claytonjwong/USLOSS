
enum { EMPTY, INUSE, BLOCKED, FREE_WILLY, TERMINATED };

enum { FALSE, TRUE };



typedef struct proc_struct* proc_ptr;


struct proc_struct{
	proc_ptr parent;
	proc_ptr next;
	proc_ptr first_child;
	proc_ptr next_sibling;
	int pid;
	int status;
	int nchild;
	int private_mbox;
	int private_buf;
	int mutex_mbox;
	int (*start_func)(void*);
	void* start_arg;
} ;




typedef struct sem_struct{
	int sid;
	int status;
	int count;
	int mutex_mbox;
	proc_ptr block_list;
} semaphore;

