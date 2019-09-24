/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>
#include <syscall.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char* progname,char** argv )
{
    //kprintf("runprogram started\n");
    
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}
        curthread->t_vmspace->v = v;

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	//vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}
        
        //kprintf("starting to count...");
        //
        //step 1: get how many arguments
        size_t argc=0;
        
        for(argc=0;(argv[argc]!=NULL)&&(argv[argc]>0x80000000);++argc){
            //kprintf("argument %u (at %#x) : %s\n",argc,argv[argc],argv[argc]);
        }
        //kprintf("argc=%u\n",argc);
        size_t *lenArray=(size_t*)kmalloc(sizeof(size_t)*argc);
        if(lenArray==NULL) return EFAULT;
        
        //step 2: get how many spaces are needed (totalsize) & get starting point of string (strstartsize)
        size_t totalsize=0;
        size_t strstartsize=0;
        int i=0;
        for(i=0;i<argc;++i){
            lenArray[i]=strlen(argv[i])+1;//length of each string
            //kprintf("lenArray[%d]=%u\n",i,lenArray[i]);
            totalsize+=lenArray[i];
        }
        if(totalsize%sizeof(userptr_t)!=0){
            strstartsize+=sizeof(userptr_t)-totalsize%sizeof(userptr_t);//padding
        }
        strstartsize+=(argc+1)*sizeof(userptr_t);//leave room for pointers (including null pointer)
        
        totalsize+=strstartsize;
        
        //kprintf("totalsize=%u,strstartsize=%u\n",totalsize,strstartsize);
        
        //step 3: copy arguments to the target stack & correct pointers
        stackptr-=totalsize;
        userptr_t writeptr=stackptr+strstartsize;
        userptr_t* pointerptr=(userptr_t*)stackptr;
        for(i=0;i<argc;++i){
            if(copyout(argv[i],writeptr,lenArray[i])!=0){
                //error handling
                kfree(lenArray);
                return EFAULT;
            }
            pointerptr[i]=writeptr;
            writeptr+=lenArray[i];
        }
        pointerptr[argc]=NULL;
        kfree(lenArray);

        //vfs_close(v);
	/* Warp to user mode. */
	md_usermode(argc /*argc*/, stackptr /*userspace addr of argv*/,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

