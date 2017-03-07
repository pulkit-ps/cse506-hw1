#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#define U_FLAG 0x01
#define A_FLAG 0x02
#define I_FLAG 0x04
#define T_FLAG 0x10
#define D_FLAG 0x20


#ifndef __NR_xmergesort
#error xmergesort system call not defined
#endif

struct input
{
	char *infile1;
	char *infile2;
	char *outfile;
	int flags;
	int *data;

};

int main(int argc,  char *argv[])
{
	int usr_flag=0,u_flag=0,a_flag=0,dflag=0;
	struct input *inp;
	char ch;
	int err=0;
	inp=malloc(sizeof(struct input));
	inp->data=malloc(sizeof(int));
	
	if (argc < 5 ){ //check if sufficient arguments passed
		err=-1;
		printf("not enough arguments passed \n");
	 	goto out;
	}	
	while ((ch=getopt(argc,argv,"uaitd")) !=-1) // code for handling flags 
	switch (ch){
	case 'u':
		u_flag =1;
		usr_flag=usr_flag | U_FLAG;
		break;
	case 'a':
                a_flag = 1;
		usr_flag=usr_flag | A_FLAG;
                break;
	case 'i':
                usr_flag=usr_flag | I_FLAG;
                break;
	case 't':
               	usr_flag=usr_flag | T_FLAG;
                break;
	case 'd':
                dflag = 1;
		usr_flag=usr_flag | D_FLAG;
                break;
	case '?':
                err=-1;
		goto out;
		break;

	}
	inp->flags=usr_flag;
	if (optind< argc){ //copy the input, output files in user space structure
         	inp->outfile =(char *) argv[optind];
		inp->infile1 =(char *) argv[optind+1];
       		inp->infile2 =(char *) argv[optind+2];
		}
	if (!(inp->infile1)||!(inp->infile2)|| !(inp->outfile)){ //if any files is missing return error
		err=-EINVAL;
		printf("Input Output files missing \n");
		goto out;
	}
	if (u_flag & a_flag){ //check if -u-a flag are simlutaneously set
		printf("invalid combination of flags\n");
		err=-1;
		goto out;

	}
	if (u_flag==0 && a_flag==0){ // pass atleast one of u or a flags
		printf("No Default beahvior please specify -u or -a \n");
		err=-1;
		goto out;
	}
	err = syscall(__NR_xmergesort,(void *)inp); //call system call xmergesort
	
	
	
out:

	if (err == 0){ //if system call is success
		printf("syscall returned %d\n", err);
		if (dflag==1) //if dflag is set the return number of records
		printf("records written =%d\n",*(inp->data));
	}
	else{ // return error if syscall fails
		
		printf("syscall failed \n");
		perror("Error is:");
	}
	exit(err);
}
