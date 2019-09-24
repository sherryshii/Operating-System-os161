 #if ndef _VM_H_
#define _VM_H_

#include <machine/vm.h>

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */


/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

struct page_frame{
    int isallocated;
    int pinned;
    int chunksize;
    
    //we dont have startphysicaladdress for every single frame struct info??????
    paddr_t startphysicaladdress;
    struct PTE* owner_table;
};

struct page_frame *coremap;
struct lock* coremap_lock;



/* Initialization function */
void vm_bootstrap(void);
void coremap_bootstrap(void);
static paddr_t getppages(unsigned long npages, int kern_user, struct PTE* owner_table);
/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);

void free_kpages(vaddr_t addr);

#endif /* _VM_H_ */
                
