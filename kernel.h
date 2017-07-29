/* ------------------------------------------------------------------------
   kernel.h

   University of Arizona
   Computer Science 452
   Summer 2003

	MASTER MINDS: Boris Salazar and Clayton Wong

   ------------------------------------------------------------------------ */

/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

struct proc_struct {
   proc_ptr       next_proc_ptr;
   proc_ptr       next_proc_ptr2;
   proc_ptr       next_proc_ptr3;
   proc_ptr       child_proc_ptr;
   proc_ptr       next_sibling_ptr;
   proc_ptr       parent_proc_ptr;
   proc_ptr       zapping_list;
   proc_ptr       child_kill_list;   // keeps track of children killed
   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   int            priority;
   int (* start_func) (char *);   /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;         /* READY, BLOCKED, QUIT, etc. */
   int            isZapped;         /* TRUE OR FALSE, etc. */
   int 						exit_code;
   int						nchild;
   unsigned int		start_time;
   unsigned int 	cpu_time;
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

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define MAX_STATUS_LEN	20


enum { FALSE=0, TRUE=1 };
enum { EMPTY, READY, RUNNING, BLOCKED_ON_JOIN, BLOCKED_ON_ZAP, QUIT, ZAPPED };

void dump_ready_list(void);

