#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include "job.h"
#define DEBUG1
int jobid=0;
int siginfo=1;
int fifo, fifo2;
int globalfd;
int wait_goon;
char print_buffer[1000];
#ifndef MY_SCHEDULER
extern struct waitqueue *head, *next;
#else
extern struct waitqueue *head[MAX_PRIORITY];
#endif

struct waitqueue *current=NULL;
//struct waitqueue *head1 = NULL;
//struct waitqueue *head2 = NULL;

void handlecmd() {
	int  count = 0;
	struct jobcmd cmd;
	struct jobinfo *newjob=NULL;
	//bzero(&cmd,DATALEN);
	memset(&cmd,0,sizeof(cmd));
	if((count=read(fifo,&cmd,DATALEN))<0)
		error_sys("read fifo failed");
#ifdef DEBUG

	if(count){
		printf("cmd cmdtype\t%d\ncmd defpri\t%d\ncmd data\t%s\n",cmd.type,cmd.defpri,cmd.data);
	}
	else
		printf("no data read\n");
#endif
	switch(cmd.type){
	case ENQ:
		// #ifdef MY_SCHEDULER
		// puts("Before enq!");
		// debug_print();
		//#endif
		do_enq(newjob,cmd);
		// #ifdef MY_SCHEDULER
		// puts("End enq!");
		// debug_print();
		// #endif
		break;
	case DEQ:
		// #ifdef MY_SCHEDULER
		// puts("Before deq!");
		// debug_print();
		// #endif
		do_deq(cmd);
		// #ifdef MY_SCHEDULER
		// puts("End deq!");
		// debug_print();
		// #endif
		break;
	case STAT:
		// #ifdef MY_SCHEDULER
		// puts("Before stat!");
		// debug_print();
		// #endif
		do_stat(cmd);
		// #ifdef MY_SCHEDULER
		// puts("End stat!");
		// debug_print();
		// #endif
		break;
	default:
		break;
	}
}

int allocjid()
{
	return ++jobid;
}

void set_wait(){
	wait_goon = 0;
}

void sig_handler(int sig,siginfo_t *info,void *notused)
{
	int status;
	int ret;
	switch (sig) {
		case SIGALRM: /* 到达计时器所设置的计时间隔 */
			scheduler();
		return;
		case SIGCHLD: /* 子进程结束时传送给父进程的信号 */

			//wait_goon = 0; // 继续运行
#ifndef MY_SCHEDULER
			if (info->si_status == SIGSTOP){
				//#ifdef DEBUG1
				puts("The job has finished its time slice but not complete!");
				debug_print();
				// #endif
				wait_goon = 0; return;
			}
			ret = waitpid(-1,&status,WNOHANG);

			if (info->si_status == SIGSTOP){
				#ifdef DEBUG1
				puts("The job has finished its time slice but not complete!");
				debug_print();
				#endif
				wait_goon = 0;
				return;
			}

			if (ret == 0)
				return;


			if(WIFEXITED(status)){
				//current->job->state = DONE; **** CRASH ****
				printf("normal termation, exit status = %d\n",WEXITSTATUS(status));
			}else if (WIFSIGNALED(status)){
				printf("abnormal termation, signal number = %d\n",WTERMSIG(status));
			}else if (WIFSTOPPED(status)){
				printf("child stopped, signal number = %d\n",WSTOPSIG(status));
			}
#endif
			return;
		default:
			return;
	}
}

void do_enq(struct jobinfo *newjob,struct jobcmd enqcmd)
{
	struct waitqueue *newnode,*p;
	int i=0,pid;
	char *offset,*argvec,*q;
	char **arglist;
	sigset_t zeromask;

	sigemptyset(&zeromask);

	/* 封装jobinfo数据结构 */
	newjob = (struct jobinfo *)malloc(sizeof(struct jobinfo));
	newjob->jid = allocjid();
	newjob->defpri = enqcmd.defpri;
	newjob->curpri = enqcmd.defpri;
	newjob->ownerid = enqcmd.owner;
	newjob->state = READY;
	newjob->create_time = time(NULL);
	newjob->wait_time = 0;
	newjob->run_time = 0;
	arglist = (char**)malloc(sizeof(char*)*(enqcmd.argnum+1));
	newjob->cmdarg = arglist;
	offset = enqcmd.data;
	argvec = enqcmd.data;
	while (i < enqcmd.argnum){
		if(*offset == ':'){
			*offset++ = '\0';
			q = (char*)malloc(offset - argvec);
			strcpy(q,argvec);
			arglist[i++] = q;
			argvec = offset;
		}else
			offset++;
	}

	arglist[i] = NULL;

	#ifdef DEBUG

	printf("enqcmd argnum %d\n",enqcmd.argnum);
	for(i = 0;i < enqcmd.argnum; i++)
		printf("parse enqcmd:%s\n",arglist[i]);

	#endif

	/*向等待队列中增加新的作业*/
#ifndef MY_SCHEDULER
	newnode = (struct waitqueue*)malloc(sizeof(struct waitqueue));
	newnode->next =NULL;
	newnode->job=newjob;

	if(head)
	{
		for(p=head;p->next != NULL; p=p->next);
		p->next =newnode;
	}else
		head=newnode;
	wait_goon = 1;
	/*为作业创建进程*/
	if((pid=fork())<0){
		error_sys("enq fork failed");
		wait_goon = 0;
	}


	if(pid==0){ // 子进程
		newjob->pid =getpid();
		/*阻塞子进程,等等执行*/
		//kill(getppid(),SIGUSR1);
		raise(SIGSTOP);
		#ifdef DEBUG

		printf("begin running\n");
		for(i=0;arglist[i]!=NULL;i++)
			printf("arglist %s\n",arglist[i]);
		#endif

		/*复制文件描述符到标准输出*/
		dup2(globalfd,1);
		/* 执行命令 */
		if(execv(arglist[0],arglist)<0){
			printf("exec failed\n");
		}

		exit(1);
	}else{ // 父进程
		while(wait_goon) sleep(1);
		//kill(pid, SIGTSTP);
		newjob->pid=pid;
	}
#else
	do_enq_native(newjob,arglist);
#endif
}

void do_deq(struct jobcmd deqcmd)
{
	int deqid,i;
	struct waitqueue *p,*prev,*select,*selectprev;
	struct waitqueue *temp1;
	deqid=atoi(deqcmd.data);

	#ifdef DEBUG
	printf("deq jid %d\n",deqid);
	#endif

#ifndef MY_SCHEDULER

	/*current jodid==deqid,终止当前作业*/
	if (current && current->job->jid ==deqid){
		temp1 = current->next;
		printf("teminate current job\n");
		kill(current->job->pid,SIGKILL);
		for(i=0;(current->job->cmdarg)[i]!=NULL;i++){
			free((current->job->cmdarg)[i]);
			(current->job->cmdarg)[i]=NULL;
		}
		free(current->job->cmdarg);
		free(current->job);
		free(current);
		current=temp1;
	}
	else{ /* 或者在等待队列中查找deqid */
		select=NULL;
		selectprev=NULL;
		if(head){
			for(prev=head,p=head;p!=NULL;prev=p,p=p->next)
				if(p->job->jid==deqid){
					select=p;
					selectprev=prev;
					break;
				}
				selectprev->next=select->next;
				if (select == selectprev)
				{
					if(select->next==NULL)
						head = NULL;
					else{
						head = select->next;
						select->next = NULL;
						}
				}	

		}
		if(select){
			for(i=0;(select->job->cmdarg)[i]!=NULL;i++){
				free((select->job->cmdarg)[i]);
				(select->job->cmdarg)[i]=NULL;
			}
			free(select->job->cmdarg);
			free(select->job);
			free(select);
			select=NULL;
		}
	}
#else
	do_deq_native(deqid);
#endif
}
// void debug_print()
// {
// 	struct waitqueue *p;
// 	char timebuf[BUFLEN];
// 	int i;
// 	if(current){
// 		strcpy(timebuf,ctime(&(current->job->create_time)));
// 		timebuf[strlen(timebuf)-1]='\0';
// 		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
// 			current->job->jid,
// 			current->job->pid,
// 			current->job->ownerid,
// 			current->job->run_time,
// 			current->job->wait_time,
// 			timebuf,"RUNNING");
// 	}
// #ifdef MY_SCHEDULER
// 	for(i=0;i<MAX_PRIORITY;++i) for(p=head[i];p;p=p->next) {
// #else
// 	for(p=head;p!=NULL;p=p->next){
// #endif
// 		strcpy(timebuf,ctime(&(p->job->create_time)));
// 		timebuf[strlen(timebuf)-1]='\0';
// 		printf("%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
// 			p->job->jid,
// 			p->job->pid,
// 			p->job->ownerid,
// 			p->job->run_time,
// 			p->job->wait_time,
// 			timebuf,
// 			"READY");
// 	}	
// }
void do_stat(struct jobcmd statcmd)
{
	struct waitqueue *p;
	char timebuf[BUFLEN];
	int shift = 0;
	int i;

	/*
	*打印所有作业的统计信息:
	*1.作业ID
	*2.进程ID
	*3.作业所有者
	*4.作业运行时间
	*5.作业等待时间
	*6.作业创建时间
	*7.作业状态
	*/

	/* 打印信息头部 */
	shift += sprintf(print_buffer,"JOBID\tPID\tOWNER\tRUNTIME\tWAITTIME\tCREATTIME\t\tSTATE\n");
	if(current){
		strcpy(timebuf,ctime(&(current->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		shift+= sprintf(print_buffer+shift,
			"%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			current->job->jid,
			current->job->pid,
			current->job->ownerid,
			current->job->run_time,
			current->job->wait_time,
			timebuf,"RUNNING");
	}
#ifdef MY_SCHEDULER
	for(i=0;i<MAX_PRIORITY;++i) for(p=head[i];p;p=p->next) {
#else
	for(p=head;p!=NULL;p=p->next){
#endif
		strcpy(timebuf,ctime(&(p->job->create_time)));
		timebuf[strlen(timebuf)-1]='\0';
		shift += sprintf(print_buffer+shift,
			"%d\t%d\t%d\t%d\t%d\t%s\t%s\n",
			p->job->jid,
			p->job->pid,
			p->job->ownerid,
			p->job->run_time,
			p->job->wait_time,
			timebuf,
			"READY");
	}

	shift ++;
	print_buffer[shift] = -1;

	if((fifo2=open("/tmp/server2",O_WRONLY))<0)
		error_sys("open fifo2 failed");
	if (write(fifo2, print_buffer, shift) < 0)
		error_sys("stat write failed");
	close(fifo2);
}

int main()
{
	struct timeval interval;
	struct itimerval new,old;
	struct stat statbuf;
	struct sigaction newact,oldact1,oldact2;
	sigset_t mask;

#ifdef MY_SCHEDULER
	puts("Modified scheduler working.");
#else
	puts("Original scheduler working.");
#endif
	if(stat("/tmp/server",&statbuf)==0){
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server")<0)
			error_sys("remove failed");
	}

	if(mkfifo("/tmp/server",0666)<0)
		error_sys("mkfifo failed");

	/* 在非阻塞模式下打开FIFO */
	if((fifo=open("/tmp/server",O_RDONLY|O_NONBLOCK))<0)
		error_sys("open fifo failed");

	/* 建立信号处理函数 */
	newact.sa_sigaction=sig_handler;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags=SA_SIGINFO;
	sigaction(SIGCHLD,&newact,&oldact1);
	sigaction(SIGALRM,&newact,&oldact2);
	signal(SIGUSR1, set_wait);
	/* 设置时间间隔为1000毫秒 */
	interval.tv_sec=1;
	interval.tv_usec=0;

	new.it_interval=interval;
	new.it_value=interval;
	setitimer(ITIMER_REAL,&new,&old);
	#ifdef DEBUG
	printf("****\n");
	#endif

	sigprocmask(0, NULL, &mask);
	sigdelset(&mask, SIGVTALRM);
	sigsuspend(&mask);
	while(1){
		if (sigsuspend(&mask) == -1){
		}
	}
	close(fifo);
	close(fifo2);
	close(globalfd);
	return 0;
}

