#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "job.h"

/*
 * 命令语法格式
 *     stat
 */
void usage()
{
	printf("Usage: stat\n");
}

int main(int argc,char *argv[])
{
	struct jobcmd statcmd;
	int fd;
	char ch;
	struct stat statbuf;

	
	if(argc!=1)
	{
		usage();
		return 1;
	}

	statcmd.type=STAT;
	statcmd.defpri=0;
	statcmd.owner=getuid();
	statcmd.argnum=0;

	if((fd=open("/tmp/server",O_WRONLY))<0)
		error_sys("stat open fifo failed");

	if(write(fd,&statcmd,DATALEN)<0)
		error_sys("stat write failed");

	close(fd);



	if(stat("/tmp/server2",&statbuf)==0){
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server2")<0)
			error_sys("remove failed");
	}
	if(mkfifo("/tmp/server2",0666)<0)
		error_sys("mkfifo failed");


	if((fd=open("/tmp/server2",O_RDONLY))<0)
		error_sys("stat open fifo failed");

	while(read(fd,&ch,1)>0 && ch!=-1){
		putchar(ch);
	}
	puts("");
	close(fd);
	return 0;
}

