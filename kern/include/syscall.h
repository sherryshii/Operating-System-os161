#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#define MAX_CHILD 1000;
/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */
struct p_list {
   
    int p_pid;
    
    struct cv *wait;
    struct addrspace *new_as;
    int exitstatus[3];
    //int my_child[1000];
};


struct p_list *process_list[300];

//lock created in bootstrap
struct lock *p_list_lock;


int sys_reboot(int code);
int sys_write (int fd, const char *buf, size_t size, int *retval);
int sys_read(int fd, char *buf, size_t buflen, int * retval);
int sys_fork(struct trapframe *tf, int *retval);
void md_forkentry(struct trapframe * new_tf, unsigned long i);
int sys_getpid(int * retval);
int sys_waitpid(int pid, int *status, int option, int *retval);
int sys_exit(int exitcode);
int sys_execv(userptr_t program, userptr_t *argv);
void* memset(void* ptr, int content, size_t size);
int sys_time(time_t *secs, u_int32_t *nsecs, int *retval);
int sys_sbrk(size_t amount, int *retval);


#endif /* _SYSCALL_H_ */
