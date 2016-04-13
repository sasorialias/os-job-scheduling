#include <stdio.h>

int main(){
	int i = 0;
	printf("(pid:%d)Demo is running\n",getpid());
	while(i<20){
		i=i+1;
		sleep(1);
		printf("(pid:%d)Demo has running %d seconds.\n", getpid(), i);
		fflush(stdout);
	}
	printf("Demo is ending.\n");
}
