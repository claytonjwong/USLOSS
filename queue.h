////////////////////////////////////////////////////////////
//
// programmers: Boris Salazar and Clayton Wong
//
// queue.h
//
// description: function declarations for queue usage
//
////////////////////////////////////////////////////////////

void enqueue(diskreq_ptr* q, diskreq_ptr req);
diskreq_ptr dequeue(diskreq_ptr* q, int headpos);
void printQ(diskreq_ptr  q);

