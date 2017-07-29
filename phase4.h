/*
 * These are the definitions for phase 4 of the project (support level, part 2).
 */

#ifndef _PHASE4_H
#define _PHASE4_H

/*
 * Maximum line length
 */

#define MAXLINE         80

/*
 * Function prototypes for this phase.
 */

extern  int  sleep(int seconds);
extern  int  disk_read (int unit, int track, int first, int sectors,
                        void *buffer);
extern  int  disk_write(int unit, int track, int first, int sectors,
                        void *buffer);
extern  int  disk_size (int unit, int *sector, int *track, int *disk);
extern  int  term_read (int unit, int size, char *buffer);
extern  int  term_write(int unit, int size, char *text);

extern  int  start4(char *);

#define ERR_INVALID             -1
#define ERR_OK                  0

#endif /* _PHASE4_H */
