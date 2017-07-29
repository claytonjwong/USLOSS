#include <phase2.h>
#include <usyscall.h>
#include <libuser.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase5.h>
#include <disk.h>

int
start5(char *arg)
{
	int rc;
	int track_dumped;
	char pukebuf[DISK_SECTOR_SIZE * DISK_TRACK_SIZE];
	char* dumpbuf = "Boris is a cool kind of guy\n";

	rc=create_bitmap(0);

	rc=disk_dump(0, DUMP_ANY, dumpbuf);
	track_dumped = rc;

	rc=disk_puke(0, track_dumped, pukebuf);

	printf("pukebuf first = \"%s\"\n", pukebuf);

	dumpbuf= "But Clayton is much cooler\n";
	rc=disk_dump(0, track_dumped, dumpbuf);

	disk_puke(0, track_dumped, pukebuf);

	printf("pukebuf second = \"%s\"\n", pukebuf);

	return 0;

} /* start5 */