#include "job.h"

#ifdef MY_SCHEDULER

#define DBG

struct waitqueue *running_stack;
struct waitqueue *head[MAX_PRIORITY];
extern struct waitqueue *current;

const int SLICE_TIME[]={5,3,2,1};

void update_all();
void job_switch();

void putss(const char *x) {
#ifdef DBG
	puts(x);
#endif
}

void push_stack(struct waitqueue* target) {
	target->next=running_stack;
	running_stack=target;
}

struct waitqueue *pop_stack(struct waitqueue **target) {
	struct waitqueue *ans=*target;
	if(*target)
		*target=(*target)->next;
	return ans;
}

void push_queue(struct waitqueue *x,struct waitqueue **target) {
	while(*target)
		target=&((*target)->next);
	x->next=NULL;
	*target=x;
}

void scheduler() {
	update_all();
	orzlibo();
	job_switch();
	printf("Executed, current task pid: %d\n",current?current->job->pid:-1);
}

int clearance;
void set_clearance() { clearance=1; }
void do_enq_native(struct jobinfo* newjob,char** arglist) {
	if(current) {
		push_stack(current);
		kill(current->job->pid,SIGSTOP);
		current->job->state=READY;
		current=NULL;
	}
	newjob->wait_time=SLICE_TIME[newjob->defpri];

	signal(SIGCONT,set_clearance);
	clearance=0;
	if((newjob->pid=fork())<0) {
		signal(SIGCONT,set_clearance);
		error_sys("enq fork failed.");
		return;
	}
	else if(newjob->pid) {
		signal(SIGCONT,set_clearance);
		struct waitqueue *tmp=
			(struct waitqueue*)malloc(sizeof(struct waitqueue));
		tmp->job=newjob;
		push_stack(tmp);
	}
	else {
		//dup2(globalfd,1);
		while(!clearance);
		if(execv(arglist[0],arglist)<0)
			puts("exec failed");
		exit(1);
	}
}

void do_deq_native(int jid) {
}

void update_all() {
	int i;
	struct waitqueue **ptr;

	for(i=MAX_PRIORITY-1;i>=0;--i)
		for(ptr=&head[i];*ptr;ptr=&(*ptr)->next)
			if(++(*ptr)->job->wait_time>=10 && i<MAX_PRIORITY-1) {
				struct waitqueue *tmp=*ptr;
				*ptr=(*ptr)->next;
				tmp->job->wait_time=0;
				push_queue(tmp,head+i+1);
			}

	if(current)
		--current->job->wait_time;
	if(current && !current->job->wait_time) { // 时间片用完了,返回等待队列进行续1秒
		push_queue(current,head + current->job->defpri);
		kill(current->job->pid,SIGSTOP);
		current->job->state=READY;
		current=NULL;
	}
}

void job_select() {
	int i;
	for(i=MAX_PRIORITY-1;i>=0&&!running_stack;--i)
		if(head[i]) {
			push_stack(pop_stack(head+i));
			running_stack->job->wait_time=
				SLICE_TIME[running_stack->job->defpri];
		}
}

void job_switch() {
	if(current) return;
	if(!running_stack) job_select();
	if(running_stack) {
		current=pop_stack(&running_stack);
		kill(current->job->pid,SIGCONT);
		current->job->state=RUNNING;
	}
}

#endif
