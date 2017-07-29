/* ------------------------------------------------------------------------
   queue.c

   University of Arizona
   Computer Science 452
   Summer 2003

	MASTER MINDS: Boris Salazar and Clayton Wong

   ------------------------------------------------------------------------ */

#include <stdio.h>
#include <driver.h>

void enqueue(diskreq_ptr* q, diskreq_ptr req) {

	diskreq_ptr itr = *q;
	req->next_req=NULL;

	if (itr == NULL){
		*q = req;
	} else {
		while(1){
			if (itr->next_req == NULL){
				break;
			}
			itr=itr->next_req;
		}
		itr->next_req = req;
	}
}

diskreq_ptr dequeue(diskreq_ptr* q, int headpos) {

	diskreq_ptr itr =NULL;
	diskreq_ptr closest =NULL;
	diskreq_ptr prev =NULL;
	diskreq_ptr prevclosest =NULL;


	if (*q==NULL){
		return NULL;
	}
	itr = *q;

	while (1) {

		if ( itr->track >= headpos )	{

			if (closest ==NULL) {
				prevclosest = prev;
				closest =itr;
			}
			else {

				if (closest->track > itr->track) {
					prevclosest = prev;
					closest = itr;
				}

			}
		}


		if (itr->next_req ==NULL)
			break;

		prev = itr;
		itr = itr->next_req;

	}

	if (closest != NULL && prevclosest!=NULL)
		prevclosest->next_req = closest->next_req;

	if (prevclosest ==NULL && closest != NULL)
		*q = closest->next_req;


	return closest;
}


void printQ(diskreq_ptr q) {

	diskreq_ptr curr = q;


	printf("[");


	while (curr != NULL) {

		printf("%d", curr->track);

		if (curr->next_req ==NULL)
			break;

		printf(", ");
		curr=curr->next_req;

	}

	printf("]\n");



}


/*

diskreq_ptr dequeue(diskreq_ptr* q, int headpos) {

	diskreq_ptr itr =NULL;
	diskreq_ptr closest =NULL;
	diskreq_ptr prev =NULL;
	diskreq_ptr prevclosest =NULL;


	if (*q==NULL){
		return NULL;
	}
	itr = *q;

	while (1) {

		if ( itr->track >= headpos )	{

			if (closest ==NULL) {
				prevclosest = prev;
				closest =itr;
			}
			else {
				if (closest->track > itr->track) {
					prevclosest = prev;
					closest = itr;
				}
			}
		}
		if (itr->next_req ==NULL)
			break;

		prev = itr;
		itr = itr->next_req;

	}

	if (closest != NULL && prevclosest!=NULL)
		prevclosest->next_req = closest->next_req;

	if (prevclosest ==NULL && closest != NULL)
		*q = closest->next_req;


	return closest;
}


*/



