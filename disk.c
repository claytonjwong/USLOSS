////////////////////////////////////////////////////////////
//
// disk.c
//
// Progammers: Boris "Superman" Salazar and Clayton Wong
//
// description: disk stuff
//
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// header files
//
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <libuser.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <disk.h>
#include <vm.h>
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// structures
//
typedef struct {
	int pid;
	int status;
} bitmap_t;
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// global
//
static bitmap_t* bitmap=NULL;
static int bitmap_mutex;
static int bitmap_init=FALSE;
static int track_no;
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// create_bitmap
//
// description: Initializes the bitmap by first asking the disk
// (unit given as parameter) how many tracks it has, which is
// then stored in the global "track_no".  Next the global "bitmap"
// is allocated track_no elements.  The bitmap is zeroed out,
// a semaphore is created for mutual exclusion when finding
// an empty track, and the global "bitmap_init" flag is set
//
// return 0 upon sucessful completion
//
// returns -1 if the call to DiskSize was given an invalid unit
// returns -2 if calloc returns NULL
//
int create_bitmap(int unit){
	int dummy;
	int rc;

	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		rc = disk_size(unit, &dummy, &dummy, &track_no);
		if (rc < 0){
			fprintf(stderr, "create_bitmap(): call to disk_size returned %d\n", rc);
		}
	} else {
		//
		// user mode
		//
		rc = DiskSize(unit, &dummy, &dummy, &track_no);
		if (rc < 0){
			fprintf(stderr, "create_bitmap(): call to DiskSize returned %d\n", rc);
		}
	}

	if (rc < 0){
		return -1;
	}

	bitmap=(bitmap_t*)calloc(track_no, sizeof(bitmap_t));
	if (bitmap==NULL){
		return -2;
	}

	//
	// zero out the bitmap
	//
	bzero(bitmap, track_no * sizeof(bitmap_t));

	//
	// create the mutex, used for
	// finding an unused track
	//
	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		bitmap_mutex = MboxCreate(1, 0);
	} else {
		//
		// user mode
		//
		Mbox_Create(1, 0, &bitmap_mutex);
	}


	//
	// set the bitmap_init flag
	// to indicate the bitmap
	// has been initialized
	//
	bitmap_init = TRUE;

	return 0;
}
//
// end create_bitmap function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// check_bitmap
//
// description: Checks to make sure the bitmap has been
// initialized by testing the bitmap_init flag
//
// returns 0 if the bitmap has been initialized
//
// returns -1 if the bitmap has not been initialized
//
int check_bitmap(char* func){
	if (! bitmap_init){
		return -1;
	}
	return 0;
}
//
// end check_bitmap function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// disk_dump
//
// description: Checks to make sure the bitmap has been
// initialized.  Then if param track_dump is set to
// DUMP_ANY, disk_dump will attempt to find an empty track
// to write param "buf" into.  Otherwise param "track_dump"
// indicates the track to write to.  It is assumed that buf's
// size is the same size as a track on the disk.
//
// returns the track written to upon sucessful completion
//         (track >= 0)
//
// returns -1 if the bitmap has not been initialized
// returns DISK_FULL if the disk is full
// returns -3 if the parameters are invalid
// returns -4 if the process does not own the track "track_dump"
//            and the track's status not TRACK_EMPTY
//
int disk_dump(int unit, int track_dump, void* buf){

	int track;
	int rc;
	int pid;
	int dummy;

	//
	// make sure the bitmap has been initialized
	//
	rc = check_bitmap("disk_dump");
	if (rc < 0){
		return -1;
	}

	if (track_dump == DUMP_ANY){
		//
		// write to a new track
		//

		///////////////////////////////
		//
		// begin mutex
		//
		if (PSR_CURRENT_MODE & psr_get()){
			//
			// kernel mode
			//
			MboxSend(bitmap_mutex, NULL, 0);
		} else {
			//
			// user mode
			//
			Mbox_Send(bitmap_mutex, 0, NULL);
		}
		//
		//
		//

		//
		// find an empty track
		//
		for (track=0; track < track_no; track++){
			if (bitmap[track].status==TRACK_EMPTY){
				break;
			}
		}

		//
		// see if there are no empty tracks
		// (disk full)
		//
		if (track==track_no){
			return DISK_FULL;
		}

		//
		// set the process id and set the status to TRACK_USED
		//
		bitmap[track].pid = getpid();
		bitmap[track].status=TRACK_USED;

		//
		//
		//
		if (PSR_CURRENT_MODE & psr_get()){
			//
			// kernel mode
			//
			MboxReceive(bitmap_mutex, NULL, 0);
		} else {
			//
			// user mode
			//
			Mbox_Receive(bitmap_mutex, 0, NULL);
		}
		//
		// end mutex
		//
		///////////////////////////////

	} else {
		//
		// overwrite the track
		//
		if (track_dump < 0
		||  track_dump >= track_no){
			//
			// invalid track
			//
			printf("disk_dump(): invalid track\n");
			return -3;
		} else {
			track = track_dump;
		}

		//
		// make sure the process owns this track
		//
		if (PSR_CURRENT_MODE & psr_get()){
			//
			// kernel mode
			//
			pid = getpid();
		} else {
			//
			// user mode
			//
			GetPID(&pid);
		}
		/*
		if (pid != bitmap[track].pid
		&&	bitmap[track].status != TRACK_EMPTY){
			return -4;
		}
		*/

		//
		// set the process id and set the status to TRACK_USED
		//
		if (PSR_CURRENT_MODE & psr_get()){
			//
			// kernel mode
			//
			bitmap[track].pid = getpid();
		} else {
			//
			// user mode
			//
			GetPID(&bitmap[track].pid);
		}
		bitmap[track].status=TRACK_USED;
	}

	//
	// take a huge dump on the disk
	//
	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		rc = disk_write(unit, track, 0, DISK_TRACK_SIZE, buf);
	} else {
		//
		// user mode
		//
		rc = DiskWrite(buf, unit, track, 0, DISK_TRACK_SIZE, &dummy);
	}

	if (rc < 0){
		printf("disk_dump(): disk_write returned %d\n", rc);
		return -3;
	}

	//
	// return the track written to
	//
	return track;
}
//
// end disk_dump
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// disk_puke
//
// description: Reads a track from the disk.  First checks
// to make sure the bitmap has been initialized.  Then the
// track is validated.  Next disk_puke checks to make sure
// the current process owns the track, and that
// the track is set to TRACK_USED, then performs
// the disk read, and stores the track information into buf
//
// returns 0 upon sucessful completion
//
// returns -1 if the bitmap has not been initialized
// returns -2 if the track is not owned by the current process
// returns -3 if the parameters are invalid
// returns -4 if the process does not own the track "track_dump"
//
int disk_puke(int unit, int track, void* buf){

	int pid;
	int rc;
	int dummy;

	//
	// make sure the bitmap has been initialized
	//
	rc = check_bitmap("disk_puke");
	if (rc < 0){
		return -1;
	}

	//
	// make sure track is valid
	//
	if (track < 0
	||  track >= track_no){
		return -3;
	}

	//
	// make sure the process who is accessing
	// the track is the correct process
	//
	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		pid = getpid();
	} else {
		//
		// user mode
		//
		GetPID(&pid);
	}
/*
	if (bitmap[track].pid != pid){
		return -4;
	}
*/
/*
	//
	// make sure the track status is TRACK_USED
	//
	if (bitmap[track].status != TRACK_USED){
		return -2;
	}
*/
	//
	// read from the disk
	//
	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		rc = disk_read(unit, track, 0, DISK_TRACK_SIZE, buf);
	} else {
		//
		// user mode
		//
		rc = DiskRead(buf, unit, track, 0, DISK_TRACK_SIZE, &dummy);
	}

	if (rc < 0){
		return -3;
	}

	return 0;
}
//
// end disk_puke function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// disk_blocks(int unit)
//
// description: returns the number of blocks (tracks)
// on the disk
//
int disk_blocks(int unit){
	int dummy;
	int blocks;
	int rc;

	if (PSR_CURRENT_MODE & psr_get()){
		//
		// kernel mode
		//
		rc = disk_size(unit, &dummy, &dummy, &blocks);
		if (rc < 0){
			fprintf(stderr, "disk_blocks(): call to disk_size returned %d\n", rc);
		}
	} else {
		//
		// user mode
		//
		rc = DiskSize(unit, &dummy, &dummy, &blocks);
		if (rc < 0){
			fprintf(stderr, "disk_blocks(): call to DiskSize returned %d\n", rc);
		}
	}

	if (rc < 0){
		return -1;
	}

	return blocks;
}
//
// end disk_blocks function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// free_block_count
//
// description: iterates through the disk bitmap
// and counts how many tracks are not in use (empty)
//
int free_block_count(int unit){
	int track;
	int free_blocks;
	int rc;

	//
	// make sure the bitmap has been initialized
	//
	rc = check_bitmap("free_block_count");
	if (rc < 0){
		return -1;
	}

	free_blocks=0;
	for (track=0; track < track_no; track++){
		if (bitmap[track].status==TRACK_EMPTY){
			free_blocks++;
		}
	}

	return free_blocks;
}
//
// end free_block_count function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// free_disk
//
// description: frees up the disk bitmap
//
void free_disk(int unit){
	free((void*)bitmap);
	bitmap=NULL;
}
//
// end free_disk function
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// print_disk
//
// description: prints the contents of the disk
//
void print_disk(int unit){
	int i;
	int rc;
	char buf[PAGE_SIZE];

	printf("track\towner\tstatus\tdata\n");

	for (i=0; i < track_no; i++){
		rc=disk_puke(unit, i, buf);
		if (rc < 0){
			fprintf(stderr, "print_disk(): disk_puke returned %d\n", rc);
		}
		printf("%d\t%d\t%d\t%s\n", i, bitmap[i].pid, bitmap[i].status, buf);
	}
}
//
// end print_disk function
////////////////////////////////////////////////////////////

