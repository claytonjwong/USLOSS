//////////////////////////////////////////////////////////////
//
// programmer: Clayton Wong
//
// mailbox.c
//
// description: kernel processes which are called through
// system call vector, these functions extract and validate
// argurments passed into sysargs, and calls the "real" function
//
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
// header files
//
#include <phase2.h> // for sysargs
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_create
//
// description: extracts and validates sysargs, then
// performs MboxCreate, which attempts to create a mailbox
// who's mailbox id is stored into mid
//
// returns: arg4=0, and arg1=mid upon sucessful completion
// returns: arg4=-1 if sysargs are invalid or there are no
//          more mailboxes available
//
void kernel_mbox_create(sysargs* sa){
	int mid;
	int nslots=(int)sa->arg1;
	int slot_sz=(int)sa->arg2;
	//
	// validate arguments
	//
	if (nslots < 0
	||  slot_sz < 0){
		sa->arg4=(void*)-1;
	}else{
		//
		// if args are valid, attempt to
		// create a mailbox
		//
		mid=MboxCreate(nslots, slot_sz);
		if (mid < 0){
			//
			// no mailboxes are available
			//
			sa->arg4=(void*)-1;
		} else {
			//
			// mailbox created sucessfully
			// return mailbox id and 0
			//
			sa->arg1=(void*)mid;
			sa->arg4=(void*)0;
		}
	}
}
//
// end kernel_mbox_create function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_release
//
// description: extracts and validates sysargs, then
// calls MboxRelease if the arguments are valid
//
// returns arg4=0 upon sucessful completion
// returns arg4=-1 if the mid was invalid, or there
//         was a problem releasing the mailbox
//
void kernel_mbox_release(sysargs* sa){
	int rc;

	int mid=(int)sa->arg1;
	rc=MboxRelease(mid);
	if (rc < 0){
		sa->arg4=(void*)-1;
	} else {
		sa->arg4=(void*)0;
	}
}
//
// end kernel_mbox_release function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_send
//
// description: extracts and validates the sysargs, then
// calls MboxSend if the arguments are valid
//
// returns arg4=0 upon sucessful completion
// returns arg4=-1 if the arguments are invalid
//         or the mailbox send failed
//
void kernel_mbox_send(sysargs* sa){
	int rc;
	int mid=(int)sa->arg1;
	void* msg=sa->arg2;
	int msg_sz=(int)sa->arg3;

	//
	// validate arguments
	//
	if (mid < 0
	||  msg_sz < 0){
		sa->arg4=(void*)-1;
	} else {
		//
		// args are valid, attempt
		// to send the message
		//
		rc=MboxSend(mid, msg, msg_sz);
		if (rc < 0){
			sa->arg4=(void*)-1;
		} else {
			sa->arg4=(void*)0;
		}
	}
}
//
// end kernel_mbox_send function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_recv
//
// description: extracts and validates sysargs, then calls
// MboxReceive if the arguments are valid.
//
// returns arg4=0 upon sucessful completion
// returns arg4=-1 if the arguments are invalid or
//         the receive on the mailbox failed
//
void kernel_mbox_recv(sysargs* sa){
	int rc;
	int mid=(int)sa->arg1;
	void* msg=sa->arg2;
	int msg_sz=(int)sa->arg3;

	//
	// validate arguments
	//
	if (mid < 0
	||  msg_sz < 0){
		sa->arg4=(void*)-1;
	} else {
		//
		// args are valid, attempt
		// to receive the message
		//
		rc=MboxReceive(mid, msg, msg_sz);
		if (rc < 0){
			sa->arg4=(void*)-1;
		} else {
			sa->arg4=(void*)0;
		}
	}
}
//
// end kernel_mbox_recv function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_condsend
//
// description: extracts and validates the sysargs, then
// calls MboxCondSend if the arguments are valid
//
// returns arg4=0 upon sucessful completion
// returns arg4=-1 if the arguments are invalid
//         or the mailbox conditional send failed
//
void kernel_mbox_condsend(sysargs* sa){
	int rc;
	int mid=(int)sa->arg1;
	void* msg=sa->arg2;
	int msg_sz=(int)sa->arg3;

	//
	// validate arguments
	//
	if (mid < 0
	||  msg_sz < 0){
		sa->arg4=(void*)-1;
	} else {
		//
		// args are valid, attempt
		// to send the message
		//
		rc=MboxCondSend(mid, msg, msg_sz);
		if (rc < 0){
			sa->arg4=(void*)-1;
		} else {
			sa->arg4=(void*)0;
		}
	}
}
//
// end kernel_mbox_condsend function
//////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////
//
// kernel_mbox_condrecv
//
// description: extracts and validates sysargs, then calls
// MboxCondReceive if the arguments are valid.
//
// returns arg4=0 upon sucessful completion
// returns arg4=-1 if the arguments are invalid or
//         the conditional receive on the mailbox failed
//
void kernel_mbox_condrecv(sysargs* sa){
	int rc;
	int mid=(int)sa->arg1;
	void* msg=sa->arg2;
	int msg_sz=(int)sa->arg3;

	//
	// validate arguments
	//
	if (mid < 0
	||  msg_sz < 0){
		sa->arg4=(void*)-1;
	} else {
		//
		// args are valid, attempt
		// to receive the message
		//
		rc=MboxCondReceive(mid, msg, msg_sz);
		if (rc < 0){
			sa->arg4=(void*)-1;
		} else {
			sa->arg4=(void*)0;
		}
	}
}
//
// end kernel_mbox_condrecv function
//////////////////////////////////////////////////////////////
