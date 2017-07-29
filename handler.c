#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include <stdlib.h>
#include <message.h>

extern int debugflag2;

/* an error method to handle invalid syscalls */
void nullsys(sysargs *args)
{
    printf("nullsys(): Invalid syscall. Halting...\n");
    halt(1);
} /* nullsys */


void clock_handler(int dev, int unit)
{
	int status;

	//
	// make sure the device is the clock device
	//
	if (dev != CLOCK_DEV){
		fprintf(stderr, "clock_handler(): dev != CLOCK_DEV\n");
		halt(1);
	}
	//
	// there is only one clock, so make sure
	// that the unit is zero
	//
	if (unit != 0){
		fprintf(stderr, "clock_handler(): unknown unit: %d\n", unit);
		halt(1);
	}
	//
	// get input from device
	//
	device_input(dev, unit, &status);
	//
	// send status of the device to the device mailbox
	//
	MboxCondSend(CLOCK_MBOX_ID,&status,sizeof(int));
} /* clock_handler */


void alarm_handler(int dev, int unit)
{


} /* alarm_handler */


void disk_handler(int dev, int unit)
{
	int status;

	if (dev != DISK_DEV){
		fprintf(stderr, "disk_handler(): dev != DISK_DEV\n");
		halt(1);
	}

	//
	// get input from device
	//
	device_input(dev, unit, &status);

	//
	// send status of the device to the device mailbox
	//
	switch(unit){
		case 1:
			MboxCondSend(DISK1_MBOX_ID,&status,sizeof(int));
			break;
		case 2:
			MboxCondSend(DISK2_MBOX_ID,&status,sizeof(int));
			break;
		default:
			fprintf(stderr, "disk_handler(): unknown unit: %d\n", unit);
			break;
	}
} /* disk_handler */


void term_handler(int dev, int unit)
{

	int status;
	int rc;

	if (dev != TERM_DEV){
		fprintf(stderr, "term_handler(): dev != TERM_DEV\n");
		halt(1);
	}

	//
	// get intput from device
	//
	rc=device_input(TERM_DEV, unit, &status);
	if (rc != DEV_OK){
		fprintf(stderr, "in term_handler(): invalid arguments to device_input()\n");
		halt(1);
	}

	//
	// set status of the device to the device mailbox
	//
	switch(unit){
		case 1:
			MboxCondSend(TERM1_MBOX_ID, &status, sizeof(int));
			break;
		case 2:
			MboxCondSend(TERM2_MBOX_ID,&status, sizeof(int));
			break;
		case 3:
			MboxCondSend(TERM3_MBOX_ID,&status, sizeof(int));
			break;
		case 4:
			MboxCondSend(TERM4_MBOX_ID,&status, sizeof(int));
			break;
		default:
			fprintf(stderr, "term_handler(): unknown unit: %d\n", unit);
			break;
	}

} /* term_handler */


void mmu_handler(int dev, int unit)
{

   if (DEBUG2 && debugflag2)
      printf("mmu_handler(): handler called\n");


} /* mmu_handler */


void syscall_handler(int dev, int unit)
{

   if (DEBUG2 && debugflag2)
      printf("syscall_handler(): handler called\n");


} /* syscall_handler */
