#include "job.h"

#ifdef MY_SCHEDULER

struct waitqueue *running_stack;
struct waitqueue *head[MAX_PRIORITY];
extern struct waitqueue *current;

void update_all();
void job_switch();

void push_stack(struct jobinfo* target) {
	target->next=running_stack;
	running_stack=target;
}

waitqueue *pop_stack(waitqueue **target) {
	waitqueue *ans=*target;
	if(*target)
		*running_stack=(*running_stack)->next;
	return ans;
}

void push_queue(waitqueue *x,waitqueue **target) {
	while(*target)
		target=&((*target)->next);
	x->next=NULL;
	*target=x;
}

void scheduler() {
	puts("Not implemented");
	update_all();
	orzlibo();
	job_switch();
}

void do_enq_native(struct jobinfo* newjob) {
}

void do_deq_native(int jid) {
}

void update_all() {
	int i;
	waitqueue **ptr;

	for(i=MAX_PRIORITY-1;i>=0;--i)
		for(ptr=&head[i];*ptr;ptr=&(*ptr)->next)
			if(++(*ptr)->job->wait_time>=10 && i<MAX_PRIORITY-1) {
				waitqueue *tmp=*ptr;
				*ptr=(*ptr)->next;
				tmp->job->wait_time=0;
				push_queue(tmp,head+i+1);
			}

	if(current)
		--current->wait_time;
	if(!current->wait_time) { // 时间片用完了,返回等待队列进行续1秒
		push_queue(current,head + current->defpri);
		kill(current->job->pid,SIGSTOP);
		current->job-state=READY;
		current=NULL;
	}
}

const int SLICE_TIME[]={5,3,2,1};
void select() {
	int i;
	for(i=MAX_PRIORITY-1;i>=0&&!running_stack;--i)
		if(head[i]) {
			push_stack(pop_stack(head+i),running_stack);
			running_stack->job->wait_time=
				SLICE_TIME[running_stack->job->defpri];
		}
}

void job_switch() {
	if(current) return;
	if(!running_stack) select();
	if(running_stack) {
		current=pop_stack(&running_stack);
		kill(current->job->pid,SIGCONT);
		current->job->state=RUNNING;
	}
}

#endif
