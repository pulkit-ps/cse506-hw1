#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/fs.h>      
#include <asm/uaccess.h>   
#include<linux/slab.h>


asmlinkage extern long (*sysptr)(void *arg);
struct input
{
	char *infile1;
	char *infile2;
	char *outfile;
	int flags;
	int *data;
	
};
int ofst;

int file_validation(char *fname1,char *fname2,char *fname3) //check files are regular and not null
{	struct kstat sb;
	int v_err=0; 
	if (!fname1 || !fname2 || !fname3){ //check if any of the files is NULL
		v_err=-EINVAL;
		goto out;
	}
	vfs_stat(fname1, &sb);
    	if ((sb.mode & S_IFMT) != S_IFREG) { //check if all files are regular
        	printk(KERN_ERR "Input file one is not a regular file\n");
         	v_err= -EINVAL;
         	goto out;
    	}
	vfs_stat(fname2, &sb);
    	if ((sb.mode & S_IFMT) != S_IFREG) {
        	printk(KERN_ERR "Input file two is not a regular file\n");
         	v_err= -EINVAL;
         	goto out;
    	}
	vfs_stat(fname3, &sb);
    	if ((sb.mode & S_IFMT) != S_IFREG) {
        	printk(KERN_ERR "Output file one is not a regular file\n");
         	v_err= -EINVAL;
         	goto out;
    	}		
out:
	return v_err;

 }


int write_file(const char *filename, void *buf,int len, struct file *filp1) //function to write the buffer to outputfile
{
    struct file *filp;
    mm_segment_t oldfs;
    int bytes;
                               
    filp = filp_open(filename, O_WRONLY | O_CREAT , filp1->f_path.dentry->d_inode->i_mode );
    if (!filp || IS_ERR(filp)) {
        printk(KERN_ERR "WRITE_file err %d\n", (int) PTR_ERR(filp));
	return -EINVAL;
	 
    }

       /* now read len bytes from offset 0 */
    filp->f_pos = ofst;
    printk("offset value %d \n", ofst);            /* start offset */
    oldfs = get_fs();
    set_fs(get_ds());
    bytes = vfs_write(filp, buf, len, &filp->f_pos);
    printk("bytes written: %d\n", bytes);
    set_fs(oldfs);
    /* close the file */
    filp_close(filp, NULL);
    return bytes;
}




asmlinkage long xmergesort(void *arg)
{	char *fname1=NULL,*tempbuf=NULL, *tempbuf2=NULL, *fname2=NULL;
	char *fname3=NULL,*str1=NULL,*str2=NULL,*buf=NULL,*buf2=NULL,*write_buf=NULL,*prev_str1=NULL, *prev_str2=NULL;
	bool complete1 = false, complete2= false, oldfs_set=false;
	int bytes_read1=0,bytes_read2=0,write_bytes,i=0, err=0, k_flag=0,len=0,len1=0,len2=0;
	int ret=0,buflen=0,count=0,retval=0,retval_t=0;
	struct file *filp1=NULL, *filp2=NULL, *filp3=NULL;
	struct input *inp=NULL;
        mm_segment_t oldfs;

	ofst=0;
	printk("Reached here\n");
	inp=kmalloc(sizeof(struct input), GFP_KERNEL);
	//reading file one	
	err=copy_from_user(inp, arg, sizeof(struct input)); //copy arguements from user space to kernel space
		
	if (err !=0){					// check if copy from user returned an error
		printk(KERN_ERR "pointer to bad address \n"); 
		err=-ENOMEM;
		goto out;
	}
	fname1=inp->infile1;
	printk("filename1 is:%s:\n", fname1);	
	fname2=inp->infile2;
	printk("filename2 is:%s:\n", fname2);
	fname3=inp->outfile;
	printk("filename3 is:%s:\n", fname3);
	k_flag=inp->flags;
	printk("flag value= %d\n",k_flag);
	retval=file_validation(fname1, fname2, fname3);	// perform  file validations on input file
	printk("return value from file validation=%d\n",retval);
	if (retval !=0){				// check if validation fails	
		printk("[xmergesort] File validation failed \n");
		err= -EINVAL;
		goto out;
	}
	buf = kmalloc((sizeof(char)*PAGE_SIZE)+1, GFP_KERNEL); // allocate memory
	if (!buf){						// check if no memory allcated return ENOMEM error
		err=-ENOMEM;
		goto out;
	}
	buf2 = kmalloc((sizeof(char)*PAGE_SIZE)+1, GFP_KERNEL);
	if (!buf2){
		err=-ENOMEM;
		goto out;
	}
	write_buf = kmalloc((sizeof(char)*(2*PAGE_SIZE))+1, GFP_KERNEL);
	if (!write_buf){
		err=-ENOMEM;
		goto out;
	}
        memset(write_buf,'\0',(2*PAGE_SIZE)+1);
	prev_str1 = kmalloc((sizeof(char)*PAGE_SIZE)+1, GFP_KERNEL); //assign buffers to store previous records 
        memset(prev_str1,'\0',PAGE_SIZE+1);
	prev_str2 = kmalloc((sizeof(char)*PAGE_SIZE)+1, GFP_KERNEL);
        memset(prev_str2,'\0',PAGE_SIZE+1);
	filp1 = filp_open(fname1, O_RDONLY, 0); // open input files 
	if (IS_ERR(filp1)){
	printk(KERN_ERR "error in opening file one\n "); // if error in opening files or read permisions not given handle the error
	err=PTR_ERR(filp1);
	goto out;
	}
        filp2 = filp_open(fname2, O_RDONLY, 0);
	
	if((filp1->f_inode==filp2->f_inode)) //check input files are 
	{	if (strcmp(filp1->f_inode->i_sb->s_type->name,filp2->f_inode->i_sb->s_type->name)==0)
        	printk(KERN_ERR " input files are same\n");
        	err=-EBADF;
        	goto out;
    	}
	filp3 = filp_open(fname3, O_WRONLY | O_CREAT ,  filp1->f_path.dentry->d_inode->i_mode); //check if input files and output files are same
	if((filp1->f_inode==filp3->f_inode)) 
        {       if (strcmp(filp1->f_inode->i_sb->s_type->name,filp3->f_inode->i_sb->s_type->name)==0)
                printk(KERN_ERR " input and output files are same\n");
                err=-EBADF;
                goto out;
        }
	if((filp2->f_inode==filp3->f_inode)) 
        {       if (strcmp(filp1->f_inode->i_sb->s_type->name,filp3->f_inode->i_sb->s_type->name)==0)
                printk(KERN_ERR " input and output files are same\n");
                err=-EBADF;
                goto out;
        }
	
        filp_close(filp3, NULL); //close write file


	filp1->f_pos = 0; //set read files offset
	filp2->f_pos = 0;
	oldfs = get_fs();
	set_fs(get_ds());
	oldfs_set=true;
	tempbuf=NULL;
	tempbuf2=NULL;
	while (!complete1 && !complete2){ //start reading and merging
		printk("Reached inside 1st while\n");
		if (tempbuf == NULL){ //if buffer one exhausted then read again from file one
			printk("inside buf1 if \n");
			if(buf != NULL){
				memset(buf,'\0',PAGE_SIZE+1);
				bytes_read1=vfs_read(filp1, buf, PAGE_SIZE, &filp1->f_pos); //read file
				if (bytes_read1 < 0){
					err=-1;
					goto out;
				}
				tempbuf=buf;
				if(bytes_read1 == PAGE_SIZE){                   //for handling partial line read
					for(i=(PAGE_SIZE-1); i>=0; i--){
						if((tempbuf[i])=='\n'){
							tempbuf[i+1]='\0';
							break;
						}
					}
					filp1->f_pos=(filp1->f_pos)-(PAGE_SIZE-i-1);
					str1=strsep(&tempbuf,"\n");
					
				}	
				else{				//file one read complelty 
					complete1=true;
					str1=strsep(&tempbuf,"\n");
				}
			}
			else{
				printk("Buf is NULL\n");
				return 0;
			}
		}
		if  (tempbuf2 == NULL){ // repeat same logic for file 2
			if(buf2 != NULL){
				memset(buf2,'\0',PAGE_SIZE+1);
				bytes_read2=vfs_read(filp2, buf2, PAGE_SIZE, &filp2->f_pos);
				if (bytes_read2 < 0){
					err=-1;
					goto out;
				}
				tempbuf2=buf2;	
				if(bytes_read2 == PAGE_SIZE){
					for(i=(PAGE_SIZE-1); i>=0; i--){ //handling partial line read 
						if((tempbuf2[i])=='\n'){
							tempbuf2[i+1]='\0';
							break;
						}
					}
					filp2->f_pos=(filp2->f_pos)-(PAGE_SIZE-i-1);
					str2=strsep(&tempbuf2,"\n");

				}
				else{
					complete2=true;
					printk("Bytes Read from file 2 !!%s\n",tempbuf2);
					str2=strsep(&tempbuf2,"\n");// extract first record from file 
				}
			}
			else{
				printk("Buf2 is NULL\n");
				return 0;
			}

            	}
		while (tempbuf != NULL && tempbuf2 != NULL){ //exit as soon as one string is null;
			if (k_flag & 0x04){   // if i flag is set comparing ignore case
				ret = strcasecmp(str1,str2);
			}
			else
				ret= strcmp(str1, str2); // else compare case sensitive
                        printk("STR1=%s,,, STR2=%s\n",str1,str2);
                if (ret <0){ //record of file one is less than record of file two
			 if (k_flag & 0x10){ // t flag logic to check file is sorted only if t flag is set
                                printk("inside T flag \n");
                                retval_t=strcmp(prev_str1,str1);
                                if (retval_t>0){
                                        printk("FILE UNSORTED Previous String11!!!=%s\n",prev_str1);
                                        printk("current String11!!!=%s\n",str1);
                                        err=-EINVAL;
                                        goto out;
                                }
                                else{
                                        memset(prev_str1,'\0',PAGE_SIZE+1);
                                        memcpy(prev_str1,str1,strlen(str1)+1 );

                                }
                        }

                         len1 = strlen(str1);
                        str1[len1] = '\n';	
			memcpy(write_buf+buflen,str1, len1+1);
			buflen=buflen+len1+1;
			count++;
                        str1=strsep(&tempbuf,"\n");
                }
                else if (ret >0){
			
			if (k_flag & 0x10){
				printk("inside T flag \n");
				retval_t=strcmp(prev_str2,str2);
				if (retval_t>0){
					printk("FILE UNSORTED Previous String22!!!=%s\n",prev_str2);	
					printk("current String22!!!=%s\n",str2);	
					err=-EINVAL;
					goto out;
					break;
				}
				else{
        				memset(prev_str2,'\0',PAGE_SIZE+1);
					memcpy(prev_str2,str2,strlen(str1)+1 );
				
				}
			}
                         len2 = strlen(str2);
                        str2[len2]='\n';
			memcpy(write_buf+buflen,str2, len2+1);
			buflen=buflen+len2+1;
			count++;
                        str2=strsep(&tempbuf2,"\n");
                }
                else{   
			if (k_flag & 0x02 ){ // if duplicate flag is set then copy both records 
				
				 len2 = strlen(str2);
                        	str2[len2]='\n';
				memcpy(write_buf+buflen,str2, len2+1);
				buflen=buflen+len2+1;
				 len = strlen(str1);
                         	str1[len] = '\n';
				memcpy(write_buf+buflen,str1, len+1);
				buflen=buflen+len+1;
				count=count+2;
                         	str1=strsep(&tempbuf,"\n");
                         	str2=strsep(&tempbuf2,"\n");

			}
			else{ //else copy only on of the rcords if records equal

                         	 len = strlen(str1);
                         	str1[len] = '\n';        
				memcpy(write_buf+buflen,str1, len+1);
				buflen=buflen+len+1;
				count++;
                         	str1=strsep(&tempbuf,"\n");
                         	str2=strsep(&tempbuf2,"\n");
			}
                }

       	 }      
}
	write_bytes=write_file(fname3,(void *) write_buf,(buflen), filp1);
	if (write_bytes<0){
		err=-EINVAL;
		goto out;
	}
	ofst=ofst+write_bytes;
	buflen=0;
        memset(write_buf,'\0',(2*PAGE_SIZE)+1);
	if (tempbuf !=NULL){                                          //write rest of the remaing buffer left after on buffer is complelety exhausted
                while (tempbuf !=NULL){
 			 len1 = strlen(str1);
                 	str1[len1] = '\n';
			memcpy(write_buf+buflen,str1, len1+1);
			buflen=buflen+len1+1;
			count++;
                	str1=strsep(&tempbuf,"\n");
               		}
	}
	if (tempbuf2 !=NULL){  // write remaining buffer if not null
                while(tempbuf2 !=NULL){ 
			 len2 = strlen(str2);
                 	str2[len2]='\n';
			printk("STR2=%s\n",str2);
			memcpy(write_buf+buflen,str2, len2+1);
			buflen=buflen+len2+1;
			count++;
			str2=strsep(&tempbuf2,"\n");
               
		
               
               		}	
	}
	//ofst+=write_file(fname3,(void *) write_buf,(buflen),filp1);
	write_bytes=write_file(fname3,(void *) write_buf,(buflen), filp1);
	if (write_bytes<0){
		err=-EINVAL;
		goto out;
	}
	ofst=ofst+write_bytes; //increase offset
	buflen=0; //set buffer lenght to zero
	printk("COUNT!!!=%d,\n",count);	
        memset(write_buf,'\0',(2*PAGE_SIZE)+1);

while (!complete1){ // read and write remaining file 1 if not read completly
        memset(buf,'\0',PAGE_SIZE+1);
        bytes_read1=vfs_read(filp1, buf, PAGE_SIZE, &filp1->f_pos);
        if (bytes_read1 < 0){
                err=-1;
                goto out;
        }
        tempbuf=buf;
        if(bytes_read1 == PAGE_SIZE){                   //for handling partial line read
                for(i=(PAGE_SIZE-1); i>=0; i--){
                        if((tempbuf[i])=='\n'){
                                tempbuf[i+1]='\0';
                                break;
                        }
                }
                filp1->f_pos=(filp1->f_pos)-(PAGE_SIZE-i-1);
                printk("Bytes Read from file 1 !!%s\n",tempbuf);
                str1=strsep(&tempbuf,"\n");
        }
        else{
                complete1=true;
                printk("Bytes Read from file 1 !!%s\n",tempbuf);
                str1=strsep(&tempbuf,"\n");
        }
        while(tempbuf !=NULL){
                len1 = strlen(str1);
                str1[len1]='\n';
                printk("STR1=%s\n",str1);
                memcpy(write_buf+buflen,str1, len1+1);
                buflen=buflen+len1+1;
                count++;
                str1=strsep(&tempbuf,"\n");
        }


}
	write_bytes=write_file(fname3,(void *) write_buf,(buflen), filp1);
	if (write_bytes<0){
		err=-EINVAL;
		goto out;
	}
	ofst=ofst+write_bytes;
	buflen=0;
	printk("COUNT!!!=%d,\n",count);
	memset(write_buf,'\0',(2*PAGE_SIZE)+1);

while (!complete2){
        memset(buf2,'\0',PAGE_SIZE+1);
        bytes_read2=vfs_read(filp2, buf2, PAGE_SIZE, &filp2->f_pos);
        if (bytes_read2 < 0){
       		err=-1;
       		goto out;
	}
        tempbuf2=buf2;
        if(bytes_read2 == PAGE_SIZE){                   //for handling partial line read
                for(i=(PAGE_SIZE-1); i>=0; i--){
                        if((tempbuf2[i])=='\n'){
                                tempbuf2[i+1]='\0';
                                break;
                        }
                }
                filp2->f_pos=(filp2->f_pos)-(PAGE_SIZE-i-1);
                printk("Bytes Read from file 2 !!%s\n",tempbuf2);
                str2=strsep(&tempbuf2,"\n");
        }
        else{
		complete2=true;
                printk("Bytes Read from file 2 !!%s\n",tempbuf2);
                str2=strsep(&tempbuf2,"\n");
        }
        while(tempbuf2 !=NULL){
		len2 = strlen(str2);
                str2[len2]='\n';
                printk("STR2=%s\n",str2);
                memcpy(write_buf+buflen,str2, len2+1);
                buflen=buflen+len2+1;
                count++;
                str2=strsep(&tempbuf2,"\n");
        }
 

}

write_bytes=write_file(fname3,(void *) write_buf,(buflen), filp1);
if (write_bytes<0){
	err=-EINVAL;
	goto out;
}
ofst=ofst+write_bytes;
buflen=0;
printk("COUNT!!!=%d,\n",count);
memset(write_buf,'\0',(2*PAGE_SIZE)+1);
err = copy_to_user(inp->data,&count,sizeof(int)); // copy d flag value that is the number of records written to userspace
if (err !=0)
{ 	err=-ENOMEM;
	printk(KERN_ERR "copy to user failed\n");
	  goto out;
}
printk("complete1= %d,,,,complete2=%d\n",complete1,complete2);	
out : //cleanup
/*delete partial output file in case of error*/
if (filp3 && !IS_ERR(filp3)) //close output file one if opened
	filp_close(filp3, NULL);
if (err <0){
filp3 = filp_open(fname3, O_WRONLY | O_CREAT ,  filp1->f_path.dentry->d_inode->i_mode); //check if input files and output files are same
vfs_unlink(filp3->f_path.dentry->d_parent->d_inode,filp3->f_path.dentry,NULL);
}

if (oldfs_set==true) //set old fs
	set_fs(oldfs);
/* close the file */
if (filp1 && !IS_ERR(filp1)) //close input file one if opened
	filp_close(filp1, NULL);
if (filp2 && !IS_ERR(filp2))	//close input file two
	filp_close(filp2, NULL);
/*empty the buffer if not null*/
if(buf != NULL)
	kfree(buf);
if(buf2 != NULL)
	kfree(buf2);
if(write_buf != NULL)
	kfree(write_buf);
if(prev_str1 != NULL)
	kfree(prev_str1);
if(prev_str2 != NULL)
	kfree(prev_str2);
printk("Return value is =%d\n",err);

return err;		
}

static int __init init_sys_xmergesort(void)
{
	printk("installed new sys_xmergesort module\n");
	if (sysptr == NULL)
		sysptr = xmergesort;
	return 0;
}
static void  __exit exit_sys_xmergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_xmergesort module\n");
}
module_init(init_sys_xmergesort);
module_exit(exit_sys_xmergesort);
MODULE_LICENSE("GPL");
