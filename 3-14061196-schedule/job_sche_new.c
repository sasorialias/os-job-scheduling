#include "job.h"

#ifdef MY_SCHEDULER

struct waitqueue *head[MAX_PRIORITY];
extern struct waitqueue *current;

void scheduler() {
	puts("Not implemented");
}

void do_enq_native(struct jobinfo* newjob) {
}

void do_deq_native(int jid) {
}

void update_all() {
}

#endif
