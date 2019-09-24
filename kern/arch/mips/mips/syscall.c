#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
#include <thread.h>
#include <synch.h>
#include <kern/errno.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/vm.h>
#include <machine/tlb.h>

#define STACKPAGES    12

/* Flags for open: choose one of these: */
#define O_RDONLY      0      /* Open for read */
#define O_WRONLY      1      /* Open for write */
#define O_RDWR        2      /* Open for read and write */
/* then or in any of these: */
#define O_CREAT       4      /* Create file if it doesn't exist */
#define O_EXCL        8      /* With O_CREAT, fail if file already exists */
#define O_TRUNC      16      /* Truncate file upon open */
#define O_APPEND     32      /* All writes happen at EOF (optional feature) */

/* Additional related definition */
#define O_ACCMODE     3      /* mask for O_RDONLY/O_WRONLY/O_RDWR */


/* Constants for read/write/etc: special file handles */
#define STDIN_FILENO  0      /* Standard input */
#define STDOUT_FILENO 1      /* Standard output */
#define STDERR_FILENO 2      /* Standard error */

/* Codes for reboot */
#define RB_REBOOT     0      /* Reboot system */
#define RB_HALT       1      /* Halt system and do not reboot */
#define RB_POWEROFF   2      /* Halt system and power off */

/* Codes for lseek */
#define SEEK_SET      0      /* Seek relative to beginning of file */
#define SEEK_CUR      1      /* Seek relative to current position in file */
#define SEEK_END      2      /* Seek relative to end of file */

/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

void
mips_syscall(struct trapframe *tf) {
    int callno;
    int32_t retval;
    int err;

    assert(curspl == 0);

    callno = tf->tf_v0;

    /*
     * Initialize retval to 0. Many of the system calls don't
     * really return a value, just 0 for success and -1 on
     * error. Since retval is the value returned on success,
     * initialize it to 0 by default; thus it's not necessary to
     * deal with it except for calls that return other values, 
     * like write.
     */

    retval = 0;

    switch (callno) {
        case SYS_reboot:
            err = sys_reboot(tf->tf_a0);
            break;

        case SYS_write:

            err = sys_write(tf->tf_a0, (const char *) tf->tf_a1, tf->tf_a2, &retval);
            break;
        case SYS_read:

            err = sys_read(tf->tf_a0, (char *) tf->tf_a1, tf->tf_a2, &retval);
            break;
        case SYS_fork:
            err = sys_fork(tf, &retval);
            break;
        case SYS_getpid:
            err = sys_getpid(&retval);
            break;
        case SYS_waitpid:
            err = sys_waitpid((int) tf->tf_a0, (int *) tf->tf_a1, tf->tf_a2, &retval);
            break;
        case SYS__exit:
            err = sys_exit(tf->tf_a0);
            break;
        case SYS_execv:
            err = sys_execv((const userptr_t) (tf->tf_a0), (const char**) (tf->tf_a1));
            break;
        case SYS___time:
            err = sys_time((time_t *) tf->tf_a0, (u_int32_t *) tf->tf_a1, &retval);
            break;
        case SYS_sbrk:
            err = sys_sbrk((size_t) tf->tf_a0, &retval);
            break;
            /* Add stuff here */


        default:
            kprintf("Unknown syscall %d\n", callno);
            err = ENOSYS;
            break;
    }


    if (err) {
        /*
         * Return the error code. This gets converted at
         * userlevel to a return value of -1 and the error
         * code in errno.
         */
        //errno = err;
        tf->tf_v0 = err;
        tf->tf_a3 = 1; /* signal an error */
    } else {
        /* Success. */
        tf->tf_v0 = retval;
        tf->tf_a3 = 0; /* signal no error */
    }

    /*
     * Now, advance the program counter, to avoid restarting
     * the syscall over and over again.
     */

    tf->tf_epc += 4;

    /* Make sure the syscall code didn't forget to lower spl */
    assert(curspl == 0);
}


//int sys_fork(struct trapframe *tf){



//}

void*
memset(void* ptr, int content, size_t size) {
    size_t i = 0;
    char* tmp = (char*) ptr;
    for (i = 0; i < size; ++i) {
        tmp[i] = (char) content;
    }
    return ptr;
}

int sys_write(int fd, const char *buf, size_t size, int *retval) {

    //kprintf("fd is %d",fd);

    int i;

    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        return EBADF;
    }
    char *bad = (char *) 0xbadbeef;
    char *bad1 = (char *) 0xdeadbeef;

    //if(buf== (char *)0xbadbeef || buf==(char *)0x0 || buf== (char *)0xdeadbeef){
    //    return 2;
    //}

    if (buf == NULL || buf == bad || buf == bad1) {
        //*retval = -1;
        return EFAULT;
    }

    char *ptr = (char *) kmalloc(sizeof (char)*(size + 1));

    int result;

    if (ptr == NULL) {
        return ENOSPC;
    }

    for (i = 0; i < size; i++) {
        if (buf[i] == NULL)
            break;
        ptr[i] = buf[i];
    }

    ptr[i] = '\0';
    int spl;
    spl = splhigh();
    kprintf("%s", ptr);
    splx(spl);
    kfree(ptr);
    *retval = size;
    return 0;


    //EIO, EBADF

}

int sys_read(int fd, char *buf, size_t buflen, int *retval) {

    if (buf == (char *) 0xbadbeef || buf == (char *) 0x0 || buf == (char *) 0xdeadbeef) {
        return EFAULT;
    }

    if (fd == STDOUT_FILENO || fd == STDERR_FILENO || fd != STDIN_FILENO) {
        return EBADF;
    }

    char *bad = (char *) 0xbadbeef;
    char *bad1 = (char *) 0xdeadbeef;


    char * ptr = (char *) kmalloc(sizeof (char)*(buflen + 1));

    if (ptr == NULL) {
        return ENOSPC;
    }

    int i = 0;
    for (i = 0; i < buflen; i++) {
        ptr[i] = getch();

        if (ptr[i] == '\n') {
            ptr[i + 1] = '\0';
            break;
        }
    }
    //kgets(ptr,buflen);
    if (i == buflen)
        ptr[i] = '\0';

    int result;
    result = copyout((const void*) ptr, (userptr_t) buf, strlen(ptr));
    kfree(ptr);

    if (result) {
        return -1;
    }

    *retval = strlen(buf);

    return 0;

}

int sys_fork(struct trapframe *tf, int32_t *retval) {

    int32_t *retpid = retval;

    int result, i, c_pid, j;

    clear_list();

    struct trapframe *new_tf = (struct trapframe *) kmalloc(sizeof (struct trapframe));

    if (new_tf == NULL) {
        return ENOMEM;
    }

    memcpy(new_tf, tf, sizeof (struct trapframe));


    struct addrspace *new_as;

    if (as_copy(curthread->t_vmspace, &new_as)) {
        kfree(new_tf);
        return ENOMEM;
    }

    lock_acquire(p_list_lock);
    //kprintf("acquire the lock\n");

    for (i = 1; i < 300; i++) {
        if (process_list[i] == NULL) {
            c_pid = i;
            process_list[i] = (struct p_list *) kmalloc(sizeof (struct p_list));
            if (process_list[i] == NULL) {
                kfree(new_tf);
                kfree(new_as);
                lock_release(p_list_lock);
                return ENOMEM;
            }

            int a = curthread->pid;
            process_list[i]->exitstatus[2] = a;
            process_list[i]->new_as = new_as;

            process_list[i]->exitstatus[0] = 0;

            break;
        }
    }

    if (i == 300) {
        kfree(new_tf);
        kfree(new_as);
        lock_release(p_list_lock);
        return EAGAIN;
    }

    process_list[i]->wait = cv_create("wait");
    lock_release(p_list_lock);
    //kprintf("process %d created\n",i);
    result = thread_fork("h", new_tf, i, md_forkentry, NULL);
    //kprintf("just called thread fork\n");

    if (result) {
        lock_acquire(p_list_lock);
        kfree(new_tf);
        kfree(process_list[i]->new_as);
        kfree(process_list[i]);
        process_list[i] = NULL;
        lock_release(p_list_lock);
        return ENOMEM;
    }

    //kfree(new_tf);
    //kfree(new_as);
    *retpid = c_pid;


    return 0;



}

void md_forkentry(struct trapframe * new_tf, unsigned long i) {

    //kprintf("coming forkentry\n");
    curthread->t_vmspace = process_list[i]->new_as;

    as_activate(curthread->t_vmspace);

    struct trapframe c_tf = *new_tf;

    kfree(new_tf);

    c_tf.tf_v0 = 0;
    c_tf.tf_a3 = 0;
    c_tf.tf_epc += 4;

    //kprintf("my pid is %d\n",curthread->pid);

    //kprintf("gping usermode\n");
    mips_usermode(&c_tf);

    panic("error return in forkentry\n");



}

int sys_getpid(int * retval) {
    *retval = curthread->pid;
    return 0;
}

int sys_waitpid(int pid, int *status, int option, int *retval) {
    int result = 1;



    lock_acquire(p_list_lock);

    //if it does not exist
    if (pid < 1 || pid > 299) {
        lock_release(p_list_lock);
        return EINVAL;
    }

    if (((int) status) % 4 != 0) {
        lock_release(p_list_lock);
        return EINVAL;
    }

    //if it does not exist
    if (process_list[pid] == NULL) {
        lock_release(p_list_lock);
        return EINVAL;
    }

    if (status == NULL || status == 0x40000000 || status == 0x80000000) {
        lock_release(p_list_lock);
        return EFAULT;
    }

    //option must be 0
    if (option != 0) {
        lock_release(p_list_lock);
        return EINVAL;
    }

    //can not wait on itself
    if (curthread->pid == pid) {
        lock_release(p_list_lock);
        return EINVAL;
    }

    //a child can not wait on its parent

    if (process_list[curthread->pid]->exitstatus[2] == pid) {
        lock_release(p_list_lock);

        return EINVAL;
    }

    //clear_list();

    //The only case allowed to call wait
    //if(process_list[pid]->exitstatus[2]==curthread->pid)


    /*check exitstatus*/

    //kprintf("%d is waiting for %d\n",curthread->pid,pid);

    //if the pid has a status quit
    if (process_list[pid]->exitstatus[0] == 1) {

        //if parent is waiting on child, erase immediately
        if (process_list[pid]->exitstatus[2] == curthread->pid) {

            *status = process_list[pid]->exitstatus[1];
            *retval = pid;

            //kfree(process_list[pid]->new_as);
            cv_destroy(process_list[pid]->wait);
            kfree(process_list[pid]);
            process_list[pid] = NULL;
            lock_release(p_list_lock);
            //kprintf("%d done waiting for %d\n",curthread->pid,pid);
            return 0;
        } else {

            *status = process_list[pid]->exitstatus[1];
            *retval = pid;
            lock_release(p_list_lock);
            //kprintf("%d done waiting for %d\n",curthread->pid,pid);
            return 0;
        }

    }

    //lock_release(p_list_lock);

    //status=0, go to sleep
    //lock_acquire(p_list_lock);

    while (process_list[pid] != NULL && process_list[pid]->exitstatus[0] == 0) {
        cv_wait(process_list[pid]->wait, p_list_lock);
    }

    //kprintf("%d done waiting for %d\n",curthread->pid,pid);

    //if is the parent coming back from sleep
    if (process_list[pid] != NULL && process_list[pid]->exitstatus[2] == curthread->pid) {

        *status = process_list[pid]->exitstatus[1];
        *retval = pid;

        //kfree(process_list[pid]->new_as);
        cv_destroy(process_list[pid]->wait);
        kfree(process_list[pid]);
        process_list[pid] = NULL;

        lock_release(p_list_lock);

        return 0;
    } else if (process_list[pid] != NULL) {

        *status = process_list[pid]->exitstatus[1];
        *retval = pid;

        lock_release(p_list_lock);

        return 0;
    } else {
        lock_release(p_list_lock);
        return 0;
    }

}

int sys_exit(int exitcode) {

    //change the status
    int pid = curthread->pid;

    lock_acquire(p_list_lock);

    process_list[pid]->exitstatus[0] = 1;
    process_list[pid]->exitstatus[1] = exitcode;

    
    cv_broadcast(process_list[pid]->wait, p_list_lock);
    lock_release(p_list_lock);
    
    thread_exit();

    return 0;
}

void clear_list() {
    int i;
    lock_acquire(p_list_lock);
    for (i = 1; i < 299; i++) {
        if (process_list[i] != NULL && process_list[i]->exitstatus[0] == 1) {
            cv_destroy(process_list[i]->wait);
            kfree(process_list[i]);
            process_list[i] = NULL;
        }
    }
    lock_release(p_list_lock);
    return;
}


#define NARG_MAX 10

int sys_execv(userptr_t program, userptr_t* argv) {

    int argc = 0;

    if (program == NULL) {
        return EFAULT;
    }
    if (program == 0x40000000 || program == 0x80000000) {
        return EFAULT;
    }

    if (argv == NULL || argv == 0x40000000 || argv == 0x80000000) {
        return EFAULT;
    }

    while (argv[argc] != NULL) {
        if (argv[argc] == 0x40000000 || argv[argc] == 0x80000000) {
            return EFAULT;
        }
        argc++;
        if (argc > NARG_MAX) {
            return EFAULT;
        }
    }

    if (strlen(program) == 0) {
        return EINVAL;
    }

    int q = 0;
    for (q = 0; q < argc; q++) {
        if (argv[q] == NULL || argv[q] == 0x40000000 || argv[q] == 0x80000000) {

            return EFAULT;
        }
    }


    size_t *lenArray = (size_t*) kmalloc(sizeof (size_t) * argc);
    if (lenArray == NULL) {
        return EFAULT;
    }

    int i = 0;
    size_t totalsize = 0;
    size_t strstartsize = 0;
    for (i = 0; i < argc; ++i) {
        lenArray[i] = strlen(argv[i]) + 1; //length of each string
        totalsize += lenArray[i];
    }
    if (totalsize % sizeof (userptr_t) != 0) {
        strstartsize += sizeof (userptr_t) - totalsize % sizeof (userptr_t); //padding
    }
    strstartsize += (argc + 1) * sizeof (userptr_t); //leave room for pointers (including null pointer)
    totalsize += strstartsize;

    char* kern_mem = (char*) kmalloc(totalsize);
    if (kern_mem == NULL) {
        kfree(lenArray);
        return 1;
    }
    //initlize memory in kernel
    //if memset cannot be used in kernel, please change this part
    memset(kern_mem, 0, totalsize);

    //put strings in reverse order in kern_mem
    char* writeptr = kern_mem + strstartsize;
    for (i = 0; i < argc; ++i) {
        if (copyin(argv[i], writeptr, lenArray[i]) != 0) {
            //error handling
            kfree(lenArray);
            kfree(kern_mem);
            return EFAULT;
        }
        writeptr += lenArray[i];
    }
    //kprintf("start creating new process...\n");
    //create new process & stack

    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    result = vfs_open(program, O_RDONLY, &v);
    if (result) {
        kfree(lenArray);
        kfree(kern_mem);
        return result;
    }


    assert(curthread->t_vmspace != NULL);

    as_destroy(curthread->t_vmspace);

    curthread->t_vmspace = as_create();


    //curthread->t_vmspace = as_create();

    if (curthread->t_vmspace == NULL) {
        vfs_close(v);
        kfree(lenArray);
        kfree(kern_mem);
        return ENOMEM;
    }
    
    curthread->t_vmspace->v = v;

    as_activate(curthread->t_vmspace);


    result = load_elf(v, &entrypoint);
    if (result) {

        vfs_close(v);
        kfree(lenArray);
        kfree(kern_mem);
        return result;
    }

    result = as_define_stack(curthread->t_vmspace, &stackptr);
    if (result) {
        kfree(lenArray);
        kfree(kern_mem);
        return result;
    }

    //kprintf("start copying back stack content...\n");

    //copy kern_mem to stack
    stackptr -= totalsize;
    if (copyout(kern_mem, stackptr, totalsize) != 0) {
        //error handling
        kfree(lenArray);
        kfree(kern_mem);
        return EFAULT;
    }


    //correct all pointers on the stack
    //kprintf("correcting pointers...\n");
    size_t strlength = strstartsize;
    for (i = 0; i < argc; ++i) {
        *((userptr_t*) stackptr + i) = (char*) (stackptr + strlength);
        strlength += lenArray[i];
    }
    *((userptr_t*) stackptr + argc) = NULL;

    kfree(lenArray);
    kfree(kern_mem);
    /*
    kprintf("done\n");
    for(i=0;i<argc;++i){
        if(strcmp((userptr_t*)stackptr+i,argv[i])!=0){
            kprintf("error for stack arguments\n");
            break;
        }
    }
     */
    //vfs_close(v);
    md_usermode(argc, stackptr, stackptr, entrypoint);


    //md_usermode(argc,(userptr_t) stackptr, stackptr, entrypoint);

    return 0;
}

int sys_time(time_t *secs, u_int32_t *nsecs, int *retval) {

    time_t ptr_secs;
    u_int32_t ptr_nsecs;

    gettime(&ptr_secs, &ptr_nsecs);

    if (secs != NULL && copyout(&ptr_secs, secs, sizeof (time_t))) {
        return EFAULT;
    }
    if (nsecs != NULL && copyout(&ptr_nsecs, nsecs, sizeof (u_int32_t))) {
        return EFAULT;
    }

    *retval = ptr_secs;

    return 0;



}

#define MAX_PAGE 32 ;
int sys_sbrk(size_t amount, int* retval) {
    

    //kprintf("amount is %x\n", amount);
    int available_page;
    int size;
    vaddr_t old_heap_break;
    //int npage;
    //int page_not_filled;

    vaddr_t stackbase = USERSTACK - STACKPAGES*PAGE_SIZE;

    struct addrspace *as;

    as = curthread->t_vmspace;
    if (as == NULL) {
        //kprintf("hshshshsh");
        
        return EFAULT;
    }   
        
    available_page = MAX_PAGE - (as->heap_end - as->heap_start)/PAGE_SIZE;
    
    size_t amount_roundup = ROUNDUP(amount, PAGE_SIZE);
    //insufficient memory
    
    if ((int)amount < 0) {
        return EINVAL;
    }
    if (amount_roundup/PAGE_SIZE > available_page) {
        return ENOMEM;
    }

    

    //fisrt access to heap
    //setting up
    if (as->heap_start == as->heap_end) {

        size = ROUNDUP(amount, PAGE_SIZE);
        as->heap_break += amount;
        as->heap_end += size;

        //the same as (end - break)
        as->page_not_filled = size - amount;

        *retval = as->heap_start;
        return 0;
    }
    else if (amount < as->page_not_filled) {

        old_heap_break = as->heap_break;
        as->heap_break += amount;
        as->page_not_filled = as->heap_end - as->heap_break;

        *retval = old_heap_break;
        return 0;
    }
    else if (amount > as->page_not_filled) {

        old_heap_break = as->heap_break;
        size = ROUNDUP(amount - as->page_not_filled, PAGE_SIZE);
        as->heap_break += amount;
        as->heap_end += size;

        *retval = old_heap_break;
        return 0;
    }


    /*
    int size;
    int npage;
    vaddr_t old_heap ;
    vaddr_t stackbase = USERSTACK - STACKPAGES*PAGE_SIZE;
    
    
    struct addrspace *as;
    if(as == NULL){
        return EFAULT;
    }
    
    as = curthread->t_vmspace;
    
    //insufficient memory
    if (as->heap_end + amount > stackbase) {
        return ENOMEM;
    }

    if (amount < 0) {
        return EINVAL;
    }


    //find the last node in linked list
    struct PTE* itr;
    struct PTE* new_PTE;
    itr = as->mytable;
    while (itr->next != NULL) {
        itr = itr->next;
    }
    //now itr points to the last element in linked list

    //heap not yet set up
    if (as->heap_start == as->heap_end) {

        //kprintf("so im the first case end=start\n");
        size = ROUNDUP(amount, PAGE_SIZE);
        npage = size / PAGE_SIZE;

        int i = 0;
        for (i = 0; i < npage; i++) {

            struct PTE* first_PTE;
            first_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            first_PTE->paddr = alloc_kpages(1) - 0x80000000;
            first_PTE->vaddr = as->heap_end + PAGE_SIZE*i;
            first_PTE->next = NULL;
            itr->next = first_PTE;
            itr = first_PTE;

        }
        old_heap = as->heap_end;
        as->heap_end += amount;
        as->heap_npage = npage;
        kprintf("old_heap is %04x \n" ,old_heap);
     *retval = old_heap;
        return 0;

    }
    //now we have first page for heap
    vaddr_t page_space_left = as->heap_start * as->heap_npage - as->heap_end;

    if (amount <= page_space_left) {
        kprintf("so amount <= pagespaceleft\n");
        old_heap = as->heap_end;
        as->heap_end += amount;
     *retval = old_heap;
        return 0;
        
    }
    if (amount > page_space_left) {
        kprintf("amount > page_space_left\n");
        size = ROUNDUP(amount, PAGE_SIZE);
        npage = size / PAGE_SIZE;

        int i = 0;
        for (i = 0; i < npage; i++) {

            struct PTE* first_PTE;
            first_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            first_PTE->paddr = alloc_kpages(1) - 0x80000000;
            first_PTE->vaddr = ROUNDUP(as->heap_end,PAGE_SIZE) + PAGE_SIZE*i;
            first_PTE->next = NULL;
            itr->next = first_PTE;
            itr = first_PTE;

        }
        old_heap = ROUNDUP(as->heap_end,PAGE_SIZE);
        as->heap_npage += npage ;
        as->heap_end = as->heap_end + page_space_left + amount;
     
     *retval = old_heap;
    return 0;

    }
     */


}
