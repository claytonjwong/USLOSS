//////////////////////////////////////////////////////////////
//
// programmer: Clayton Wong
//
// mailbox.h
//
// description: user system calls go through the system vector
// which points to these functions, and performs these functions
// in kernel mode
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// function declarations
//
void kernel_mbox_create(sysargs* sa);
void kernel_mbox_release(sysargs* sa);
void kernel_mbox_send(sysargs* sa);
void kernel_mbox_recv(sysargs* sa);
void kernel_mbox_condsend(sysargs* sa);
void kernel_mbox_condrecv(sysargs* sa);
//////////////////////////////////////////////////////////////
