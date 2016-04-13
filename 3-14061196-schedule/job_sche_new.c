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

void free_item(struct waitqueue *target) {
	puts("not implemented (memory leak)");
	free(target->job);
	free(target);
}

inline void push_stack(struct waitqueue* target) {
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
void send_back_to_queue() {
	if(!current) return;
	kill(current->job->pid,SIGSTOP);
	current->job->wait_time=0;
	current->job->state=READY;
	if(waitpid(current->job->pid,NULL,WNOHANG))
		free_item(current);
	else
		push_queue(current,&head[current->job->defpri]);
	current=NULL;
}

void scheduler() {
	update_all();
	orzlibo();
	job_switch();
}

int clearance;
void set_clearance() { clearance=1; }
void do_enq_native(struct jobinfo* newjob,char** arglist) {
	if(current && newjob->defpri > current->job->defpri) 
		send_back_to_queue();

	signal(SIGCONT,set_clearance);
	clearance=0;
	fflush(stdout);
	if((newjob->pid=fork())<0) {
		signal(SIGCONT,SIG_DFL);
		error_sys("enq fork failed.");
		return;
	}
	else if(newjob->pid) {
		signal(SIGCONT,SIG_DFL);
		kill(newjob->pid,SIGSTOP);
		struct waitqueue *tmp=
			(struct waitqueue*)malloc(sizeof(struct waitqueue));
		tmp->job=newjob;
		push_queue(tmp,&head[newjob->defpri]);
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
	printf("prepare to deq jid=%d\n",jid);
	int i;
	struct waitqueue **ptr;
	if(current && current->job->jid==jid)
		send_back_to_queue();
	for(i=0;i<MAX_PRIORITY;++i)
		for(ptr=head+i;*ptr;ptr=&(*ptr)->next)
			if((*ptr)->job->jid==jid) {
				struct waitqueue *tmp=*ptr;
				*ptr=(*ptr)->next;
				free_item(tmp);
				return;
			}
	puts("job not found");
}

void update_all() {
	int i;
	struct waitqueue **ptr;

	for(i=MAX_PRIORITY-1;i>=0;--i)
		for(ptr=&head[i];*ptr;*ptr&&(ptr=&(*ptr)->next))
			if(++(*ptr)->job->wait_time>=10 && i<MAX_PRIORITY-1) {
				struct waitqueue *tmp=*ptr;
				*ptr=(*ptr)->next;
				tmp->job->wait_time=0;
				push_queue(tmp,head+i+1);
			}
	if(current && !--current->job->wait_time)
		send_back_to_queue();
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
	if(current) {
		if(waitpid(current->job->pid,NULL,WNOHANG))
			send_back_to_queue();
		else return;
	}
	if(!running_stack) job_select();
	if(running_stack) {
		current=pop_stack(&running_stack);
		kill(current->job->pid,SIGCONT);
		current->job->state=RUNNING;
	}
}

#endif
