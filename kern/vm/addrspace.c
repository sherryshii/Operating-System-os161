#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <uio.h>
#include <vnode.h>

#define O_RDONLY      0      /* Open for read */
#define O_WRONLY      1      /* Open for write */
#define O_RDWR        2      /* Open for read and write */

#define SWAP_DEVICE "lhd0raw:" 

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground. You should replace all of this
 * code while doing the VM assignment. In fact, starting in that
 * assignment, this file is not included in your kernel!
 */

/* under dumbvm, always have 48k of user stack */

int npage_frame;
int coremap_initialized = 0;
struct vnode* myvnode;
int slot_count = 0;
struct vnode* myvnode;

void
vm_bootstrap(void) {

    /*
    char *name = kstrdup("DISK1.img");
    myvnode = (struct vnode*)kmalloc(sizeof(struct vnode));

    int result = vfs_open(name, O_RDWR, &myvnode);
    //assert(myvnode->vn_ops != NULL);
    if (result) {
        kprintf("read failed!!\n");
        kfree(name);
    }
     */

    int result;
    myvnode = (struct vnode*) kmalloc(sizeof (struct vnode));
    result = vfs_open(SWAP_DEVICE, O_RDWR, &myvnode);

    assert(myvnode->vn_ops != NULL);
    if (result) {
        kprintf("read failed!!\n");
        //kfree(name);
    }




}

void coremap_bootstrap() {
 

    paddr_t firstpaddr, lastpaddr;

    ram_getsize(&firstpaddr, &lastpaddr);

    npage_frame = lastpaddr / PAGE_SIZE;
    //struct page_frame *coremap = (struct page_frame *)kmalloc(sizeof(struct page_frame) * npage_frame);
    firstpaddr = ROUNDUP(firstpaddr, PAGE_SIZE);
    coremap = (struct page_frame*) PADDR_TO_KVADDR(firstpaddr);
    int coremap_size = npage_frame * (sizeof (struct page_frame));
    coremap_size = ROUNDUP(coremap_size, PAGE_SIZE);
    int coremap_page = coremap_size / PAGE_SIZE;
    //paddr_t coremap_firstaddress;
    //coremap_firstaddress = firstpaddr;

    int index = firstpaddr / PAGE_SIZE + coremap_page;


    int i = 0;
    for (i = 0; i < npage_frame; i++) {
        if (i < index) {
            coremap[i].isallocated = 1;
            coremap[i].pinned = 1;
            coremap[i].startphysicaladdress = i*PAGE_SIZE;
            coremap[i].chunksize = 0;
            coremap[i].owner_table = NULL;
        } else {
            coremap[i].isallocated = 0;
            coremap[i].pinned = 0;
            coremap[i].startphysicaladdress = i*PAGE_SIZE;
            coremap[i].chunksize = 0;
            coremap[i].owner_table = NULL;
        }
    }

    coremap_initialized = 1;


}

paddr_t
getppages(unsigned long npages, int kern_user, struct PTE* myPTE) {

    if (coremap_initialized == 1) {

        int spl;
        paddr_t addr;

        spl = splhigh();

        int i = 0, j = 0, bad = 0, bad1 = 0;

        if (npages > 1) {
            for (i = 0; i < npage_frame; i++) {
                bad = 0;
                if (coremap[i].isallocated == 0) {
                    for (j = 0; j < npages; j++) {

                        if (coremap[j + i].isallocated == 0) {
                            continue;
                        } else {
                            bad = 1;
                            break;
                        }
                    }
                    if (bad == 1) {
                        continue;
                    } else {
                        int k = 0;
                        for (k = i; k < npages + i; k++) {
                            coremap[k].isallocated = 1;
                            coremap[k].pinned = kern_user; //is a kernel/user page
                            //if (myPTE != NULL) {
                            //    coremap[k].owner_table = myPTE;
                            //}

                        }
                        coremap[i].chunksize = npages;
                        splx(spl);
                        return coremap[i].startphysicaladdress;
                    }
                }
            }
            /*
            int go, gg, bad1;
            for (go = 0; go < npage_frame; go++) {
                if (coremap[go].pinned == 0) {
                    bad1 = 0;
                    for (gg = 0; gg < npages; gg++) {
                        if (coremap[gg + 1].pinned == 0) {
                            continue;
                        } else {
                            bad1 = 1;
                            break;
                        }
                    }
                    if (bad1 == 1) {
                        continue;
                    } else {
                        int c = 0;
                        for (c = gg; c < npages + gg; c++) {

                            //swapping one by one
                            //first get the physical address to pass in to uio struct
                            paddr_t paddr_c = coremap[c].startphysicaladdress;
                            vaddr_t verkeraddr_c = PADDR_TO_KVADDR(paddr_c);

                            //notifying the owener that this is going to be swapped out
                            coremap[c].owner_table->swapped = 1;
                            coremap[c].owner_table->slot_disk = slot_count;
                            slot_count++;

                            struct uio uio_c;
                            mk_kuio(&uio_c, &verkeraddr_c, PAGE_SIZE, PAGE_SIZE*slot_count, UIO_WRITE);

                            VOP_WRITE(myvnode, &uio_c);

                            coremap[c].pinned = kern_user;
                            //coremap[c].chunksize = 1;
                            //coremap[c].startphysicaladdress = paddr;
                            coremap[c].isallocated = 1;
                            //i dont think user will get there
                            coremap[c].owner_table = NULL;
                        }
                        coremap[gg].chunksize = npages;
                        splx(spl);
                        return coremap[gg].startphysicaladdress;
                    }
                }
            }*/
            //panic("out of memoryyyyyyy");

        } else if (npages == 1) {
            for (i = 0; i < npage_frame; i++) {
                if (coremap[i].isallocated == 0) {
                    coremap[i].isallocated = 1;
                    coremap[i].chunksize = 1;
                    coremap[i].pinned = kern_user; //is a kernel/user page
                    if (myPTE != NULL) {
                        coremap[i].owner_table = myPTE;
                    }
                    splx(spl);
                    return coremap[i].startphysicaladdress;
                }
            }
            //panic("out of memory!!!!!!!!!!!!!!!!");
            //kprintf("swap out needed\n");
            int gogo;

            while (1) {
                gogo = random() % 128;
                if (coremap[gogo].pinned == 0) {
                    break;
                } else if (coremap[gogo].pinned == 1) {
                    continue;
                }
            }

            //for (gogo = 0; gogo < npage_frame; gogo++) {
            //find a page that is not pinned and swap it out
            //if pinned = 0, means its not a kernel page and will never has a chunksize > 1

            if (coremap[gogo].pinned == 0) {
                //kprintf("found someone to kill\n");
                //once it found out a page to evict
                //the index of the evicting page is current gogo
                //this physical address is the frame that i want to swap content out
                paddr_t paddr = coremap[gogo].startphysicaladdress;

                //so this page is going to be swapped out 
                //has to notify the owner table change its PTE
                //not sure if i should clear the paddr in this PTE???????????????
                coremap[gogo].owner_table->swapped = 1;


                int position;
                if (coremap[gogo].owner_table->slot_disk == -1) {
                    if (slot_count >= 1279) {
                        splx(spl);
                        return ENOMEM;
                    }
                    coremap[gogo].owner_table->slot_disk = slot_count;
                    position = slot_count;
                    slot_count++;
                    //kprintf("current slot count: %d\n",slot_count);
                } else {
                    position = coremap[gogo].owner_table->slot_disk;
                }

                //coremap[gogo].owner_table->slot_disk = slot_count;
                //kprintf("all good so far\n");


                //now ready to be swapped out
                struct uio myuio; //= (struct uio*)kmalloc(sizeof(struct uio));
                //trans the physical address to kernel virtual address
                vaddr_t kerveraddr = PADDR_TO_KVADDR(paddr);
                //kprintf("ready to write\n");
                mk_kuio(&myuio, kerveraddr, PAGE_SIZE, PAGE_SIZE*position, UIO_WRITE);
                //kprintf("done load\n");
                slot_count++;

                assert(myuio.uio_rw == UIO_WRITE);

                //kprintf(" uio rw is %s \n" ,myuio->uio_rw);

                VOP_WRITE(myvnode, &myuio);


                //kprintf("done write\n");
                //now the thing in paddr is moved to disk 
                //get me the page 
                //im the one who make this call
                //occupy the space just got evicted
                //myPTE->paddr = paddr; (dont have to do it here cuz i will return this paddr to the call)

                coremap[gogo].pinned = kern_user;
                coremap[gogo].chunksize = 1;
                coremap[gogo].startphysicaladdress = paddr;
                coremap[gogo].isallocated = 1;
                coremap[gogo].owner_table = myPTE;

                //kprintf("done swap out\n");
                splx(spl);
                return coremap[gogo].startphysicaladdress;
            }




        }

    }

    if (coremap_initialized == 0) {

        int spl;
        paddr_t addr;

        spl = splhigh();

        addr = ram_stealmem(npages);

        splx(spl);
        return addr;
    }
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(int npages) {
    paddr_t pa;

    //pass in one means this is from 
    pa = getppages(npages, 1, NULL);



    if (pa == 0) {
        return 0;
    }
    return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr) {


    paddr_t pa;
    pa = addr - MIPS_KSEG0;


    assert(addr == PADDR_TO_KVADDR(pa));

    int i = 0;
    int j = 0;
    for (i = 0; i < npage_frame; i++) {
        if (coremap[i].startphysicaladdress == pa) {
            if (coremap[i].chunksize == 1) {
                coremap[i].isallocated = 0;
                

            } else {
                for (j = 0; j < coremap[i].chunksize; j++) {
                    coremap[j].isallocated = 0;
                    //char* tmp = coremap[j].startphysicaladdress;
                    //memset(tmp, 0, PAGE_SIZE);
                }
            }
        }


    }
}

int
vm_fault(int faulttype, vaddr_t faultaddress) {
    //kprintf("entering vm_fault  %04x\n",faultaddress);
    vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
    paddr_t paddr;
    //paddr_t mypaddr;
    int i;
    u_int32_t ehi, elo;
    struct addrspace *as;
    struct uio myuio;
    int spl;

    spl = splhigh();

    //take upper bits rest 0
    faultaddress &= PAGE_FRAME;

    DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

    switch (faulttype) {
        case VM_FAULT_READONLY:
            /* We always create pages read-write, so we can't get this*/
            panic("dumbvm: got VM_FAULT_READONLY\n");
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            break;
        default:
            splx(spl);
            return EINVAL;
    }

    as = curthread->t_vmspace;
    if (as == NULL) {
        /*
         * No address space set up. This is probably a kernel
         * fault early in boot. Return EFAULT so as to panic
         * instead of getting into an infinite faulting loop.
         */
        return EFAULT;
    }

    /* Assert that the address space has been set up properly.*/
    assert(as->region1 != 0);
    assert(as->region2 != 0);
    assert(as->npage_region1 != 0);
    assert(as->npage_region2 != 0);
    
    //assert(as->mytable != NULL);

    assert((as->region1 & PAGE_FRAME) == as->region1);
    assert((as->region2 & PAGE_FRAME) == as->region2);
    assert(as->v != NULL);
    vbase1 = as->region1;
    vtop1 = vbase1 + as->npage_region1 * PAGE_SIZE;
    vbase2 = as->region2;
    vtop2 = vbase2 + as->npage_region2 * PAGE_SIZE;
    stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;
    //kprintf("stackbase is %04x\n" ,stackbase);
    //kprintf("stacktop is %04x\n" ,stacktop);

    //if it the vaddr fall in region1
    if (faultaddress >= vbase1 && faultaddress < vtop1) {
        
        //kprintf("region1 faultaddress: %04x\n",faultaddress);

        struct PTE* tmp;
        tmp = as->mytable;

        //go through the page table to find if there is a node exist
        //if exist, then it must be mapped
        //check if its in memory or swapped
        //if in memory, just break
        //if being swapped, swap back in
        while (tmp != NULL) {
            if (tmp->vaddr == faultaddress) {

                //now we find the node
                //see where it is
                if (tmp->swapped == 0) {
                    //it is in memory
                    paddr = tmp->paddr;
                    break;
                } else if (tmp->swapped == 1) {
                    //its been swapped
                    //need it back

                    //first get a physical page
                    tmp->paddr = getppages(1, 0, tmp);

                    //only need to pass the position and how long byte to read 

                    mk_kuio(&myuio, PADDR_TO_KVADDR(tmp->paddr), PAGE_SIZE, (tmp->slot_disk) * PAGE_SIZE, UIO_READ);
                    VOP_READ(myvnode, &myuio);

                    //back to memory
                    //dont know what to do with coremap here
                    tmp->swapped = 0;

                    paddr = tmp->paddr;
                    break;

                }


            }
            tmp = tmp->next;

        }

        if (tmp == NULL) {
            
            //kprintf("load segment 1\n");

            struct PTE* new_PTE;
            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            new_PTE->paddr = getppages(1, 0, new_PTE);
            new_PTE->vaddr = faultaddress;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;
            
            if (as->mytable == NULL) {
                as->mytable = new_PTE;
            } else {
                tmp = as->mytable;
                while (tmp->next != NULL) {
                    tmp = tmp->next;
                }
                tmp->next = new_PTE;
            }

            paddr = new_PTE->paddr;

            size_t memsize = as->memsize1;
            size_t filesize = as->filesize1;
            size_t vdiff = faultaddress - vbase1;
            size_t offset = as->offset1;

            //int mempage = ROUNDUP(memsize, PAGE_SIZE) / PAGE_SIZE;
            //int filepage = ROUNDUP(filesize, PAGE_SIZE) / PAGE_SIZE;
            //kprintf("vdiff is %d\n" ,vdiff);
            int result;

            size_t pmemsize, pfilesize, poffset;
            if (memsize > vdiff + PAGE_SIZE) {
                pmemsize = PAGE_SIZE;
            } else if (memsize > vdiff) {
                pmemsize = memsize - vdiff;
            } else {
                splx(spl);
                return EFAULT;
            }

            if (filesize > vdiff + PAGE_SIZE) {
                pfilesize = PAGE_SIZE;
            } else if (filesize >= vdiff) {
                pfilesize = filesize - vdiff;
            } else {
                pfilesize = 0;
            }

            poffset = vdiff + offset;
            
            //kprintf("vdiff: %04x pfilesize: %04x pmemsize: %04x poffset: %04x\n",vdiff,pfilesize,pmemsize,poffset);

            result = load_page_segment(as->v, poffset, faultaddress, pmemsize, pfilesize, as->executable1);
            if (result) {
                splx(spl);
                return EFAULT;
            }

            splx(spl);
            return 0;

        }





    } else if (faultaddress >= vbase2 && faultaddress < vtop2) {
            struct PTE* tmp;
        tmp = as->mytable;
        
        //kprintf("region2 faultaddress: %04x\n",faultaddress);

        //go through the page table to find if there is a node exist
        //if exist, then it must be mapped
        //check if its in memory or swapped
        //if in memory, just break
        //if being swapped, swap back in
        while (tmp != NULL) {
            if (tmp->vaddr == faultaddress) {

                //now we find the node
                //see where it is
                if (tmp->swapped == 0) {
                    //it is in memory
                    paddr = tmp->paddr;
                    break;
                } else if (tmp->swapped == 1) {
                    //its been swapped
                    //need it back

                    //first get a physical page
                    tmp->paddr = getppages(1, 0, tmp);

                    //only need to pass the position and how long byte to read 

                    mk_kuio(&myuio, PADDR_TO_KVADDR(tmp->paddr), PAGE_SIZE, (tmp->slot_disk) * PAGE_SIZE, UIO_READ);
                    VOP_READ(myvnode, &myuio);

                    //back to memory
                    //dont know what to do with coremap here
                    tmp->swapped = 0;

                    paddr = tmp->paddr;
                    break;

                }


            }
            tmp = tmp->next;

        }

        if (tmp == NULL) {
            
            //kprintf("load segment 2\n");

            struct PTE* new_PTE;
            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            new_PTE->paddr = getppages(1, 0, new_PTE);
            new_PTE->vaddr = faultaddress;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;
            
            if (as->mytable == NULL) {
                as->mytable = new_PTE;
            } else {
                tmp = as->mytable;
                while (tmp->next != NULL) {
                    tmp = tmp->next;
                }
                tmp->next = new_PTE;
            }

            paddr = new_PTE->paddr;

            size_t memsize = as->memsize2;
            size_t filesize = as->filesize2;
            size_t vdiff = faultaddress - vbase2;
            size_t offset = as->offset2;

            int mempage = ROUNDUP(memsize, PAGE_SIZE) / PAGE_SIZE;
            int filepage = ROUNDUP(filesize, PAGE_SIZE) / PAGE_SIZE;
            int result;

            size_t pmemsize, pfilesize, poffset;
            if (memsize > vdiff + PAGE_SIZE) {
                pmemsize = PAGE_SIZE;
            } else if (memsize > vdiff) {
                pmemsize = memsize - vdiff;
            } else {
                splx(spl);
                return EFAULT;
            }

            if (filesize > vdiff + PAGE_SIZE) {
                pfilesize = PAGE_SIZE;
            } else if (filesize >= vdiff) {
                pfilesize = filesize - vdiff;
            } else {
                pfilesize = 0;
            }

            poffset = vdiff + offset;
            //kprintf("vdiff: %04x pfilesize: %04x pmemsize: %04x poffset: %04x\n",vdiff,pfilesize,pmemsize,poffset);
            result = load_page_segment(as->v, poffset, faultaddress, pmemsize, pfilesize, as->executable2);
            if (result) {
                splx(spl);
                return EFAULT;
            }

            splx(spl);
            return 0;

        }


    } else if (faultaddress >= stackbase && faultaddress < stacktop) {

    
        //kprintf("entering stack region, fault addr: %04x\n",faultaddress);
        struct PTE*tmp;
        tmp = as->mytable;

        if (tmp == NULL) {

            struct PTE* new_PTE;
            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            new_PTE->paddr = getppages(1, 0, new_PTE);
            new_PTE->vaddr = faultaddress;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;

            as->mytable = new_PTE;
            paddr = new_PTE->paddr;
            
            goto done;
        }

        while (tmp != NULL) {
            if (tmp->vaddr == faultaddress) {
                //now we find the node
                //see where it is
                if (tmp->swapped == 0) {
                    //it is in memory
                    paddr = tmp->paddr;
                    break;
                } else if (tmp->swapped == 1) {
                    //its been swapped
                    //need it back

                    //first get a physical page
                    tmp->paddr = getppages(1, 0, tmp);

                    //only need to pass the position and how long byte to read 

                    mk_kuio(&myuio, PADDR_TO_KVADDR(tmp->paddr), PAGE_SIZE, (tmp->slot_disk) * PAGE_SIZE, UIO_READ);
                    VOP_READ(myvnode, &myuio);

                    //back to memory
                    //dont know what to do with coremap here
                    tmp->swapped = 0;

                    paddr = tmp->paddr;
                    break;

                }
            }
            tmp = tmp->next;
            if (tmp == NULL) {
                //kprintf("new stack page created\n");
                struct PTE* itr;
                itr = as->mytable;
                while (itr->next != NULL) {
                    itr = itr->next;
                }

                struct PTE* new_PTE;
                new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
                new_PTE->paddr = getppages(1, 0, new_PTE);
                new_PTE->vaddr = faultaddress;
                new_PTE->next = NULL;
                new_PTE->swapped = 0;
                new_PTE->slot_disk = -1;

                itr->next = new_PTE;
                paddr = new_PTE->paddr;
                //kprintf("paddr is %04x\n", paddr);
                break;

            }

        }



    } else if (faultaddress <= as->heap_break && faultaddress >= as->heap_start) {

      // kprintf("entering heap region\n");

        struct PTE*tmp;
        tmp = as->mytable;
        
        if (tmp == NULL) {

            struct PTE* new_PTE;
            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            new_PTE->paddr = getppages(1, 0, new_PTE);
            new_PTE->vaddr = faultaddress;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;

            as->mytable = new_PTE;
            paddr = new_PTE->paddr;
            
            goto done;
        }

        while (tmp != NULL) {
            if (tmp->vaddr == faultaddress) {
                //now we find the node
                //see where it is
                if (tmp->swapped == 0) {
                    //it is in memory
                    paddr = tmp->paddr;
                    break;
                } else if (tmp->swapped == 1) {
                    //its been swapped
                    //need it back

                    //first get a physical page
                    tmp->paddr = getppages(1, 0, tmp);

                    //only need to pass the position and how long byte to read 

                    mk_kuio(&myuio, PADDR_TO_KVADDR(tmp->paddr), PAGE_SIZE, (tmp->slot_disk) * PAGE_SIZE, UIO_READ);
                    VOP_READ(myvnode, &myuio);

                    //back to memory
                    //dont know what to do with coremap here
                    tmp->swapped = 0;

                    paddr = tmp->paddr;
                    break;

                }
            }
            tmp = tmp->next;
            if (tmp == NULL) {

                struct PTE* itr;
                itr = as->mytable;
                while (itr->next != NULL) {
                    itr = itr->next;
                }

                struct PTE* new_PTE;
                new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
                new_PTE->paddr = getppages(1, 0, new_PTE);
                new_PTE->vaddr = faultaddress;
                new_PTE->next = NULL;
                new_PTE->swapped = 0;
                new_PTE->slot_disk = -1;
                itr->next = new_PTE;
                paddr = new_PTE->paddr;
                //kprintf("paddr is %04x\n", paddr);
                break;

            }
        }



    } else {
        splx(spl);
        return EFAULT;
    }

    //kprintf("all good so far\n");
    done:
    /* make sure it's page-aligned*/
    assert((paddr & PAGE_FRAME) == paddr);

    for (i = 0; i < NUM_TLB; i++) {

        TLB_Read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }

        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        TLB_Write(ehi, elo, i);

        splx(spl);
        return 0;
    }

    kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
    splx(spl);
    return EFAULT;


}

struct addrspace *
as_create(void) {



    struct addrspace *as = kmalloc(sizeof (struct addrspace));
    if (as == NULL) {
        return NULL;
    }

    as->region1 = 0;
    as->npage_region1 = 0;

    as->region2 = 0;
    as->npage_region2 = 0;

    as->heap_start = 0;
    as->heap_end = 0;
    as->heap_npage = 0;
    as->heap_break = 0;
    as->page_not_filled = 0;

    as->memsize1 = 0;
    as->memsize2 = 0;
    as->filesize1 = 0;
    as->filesize2 = 0;
    as->executable1 = 0;
    as->executable2 = 0;
    as->offset1 = 0;
    as->offset2 = 0;
    as->v = NULL;
    as->mytable = NULL;

    return as;

}

void
as_destroy(struct addrspace * as) {

    //kfree(as);
    int index;


    struct PTE* current;
    struct PTE* tokill;

    current = as->mytable;

    if (current == NULL) {
        kfree(as);
        return;
    }

    while (current->next != NULL) {

        tokill = current;
        current = current->next;
        index = tokill->paddr / PAGE_SIZE;
        coremap[index].isallocated = 0;
        coremap[index].pinned = 0;
        coremap[index].chunksize = 0;
        coremap[index].owner_table = NULL;
        kfree(tokill);
    }
    index = current->paddr / PAGE_SIZE;
    coremap[index].isallocated = 0;
    coremap[index].chunksize = 0;
    coremap[index].pinned = 0;
    coremap[index].owner_table = NULL;
    kfree(current);


    kfree(as);

    return;

}

void
as_activate(struct addrspace * as) {
    int i, spl;

    (void) as;

    spl = splhigh();

    for (i = 0; i < NUM_TLB; i++) {
        // write the TLB entry specified by ENTRYHI and ENTRYLO to index i
        //#define TLBHI_INVALID(entryno) ((0x80000+(entryno))<<12)
        // #define TLBLO_INVALID()        (0)
        TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
        int readable, int writeable, int executable) {


    size_t npages;

    /* Align the region. First, the base... */
    sz += vaddr & ~(vaddr_t) PAGE_FRAME; //lower 12 bits of vaddr which is equal to (vaddr % PAGE_SIZE)
    vaddr &= PAGE_FRAME;

    /* ...and now the length.*/
    sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = sz / PAGE_SIZE;

    /* We don't use these - all pages are read-write */
    (void) readable; //4
    (void) writeable; //2
    (void) executable; //1
    /* record vnode for future read from disk to memory */

    if (as->region1 == 0) {
        as->region1 = vaddr;
        as->npage_region1 = npages;
        //as->read1 = readable;
        //as->write1 = writable;
        //as->executable1 = executable;
        return 0;
    }

    if (as->region2 == 0) {
        as->region2 = vaddr;
        as->npage_region2 = npages;
        //as->read2 = readable;
        //as->write2 = writable;
        //as->executable2 = executable;

        as->heap_start = as->region2 + as->npage_region2*PAGE_SIZE;
        as->heap_end = as->heap_start;
        as->heap_break = as->heap_start;

        return 0;

    }


    /*
     * Support for more than two regions is not available.
     */
    kprintf("dumbvm: Warning: too many regions\n");
    return EUNIMP;
}

int
as_prepare_load(struct addrspace * as) {

    //assert(as->as_stackpbase == 0);

    struct PTE *itr;
    struct PTE *new_PTE;
    if (as->mytable == NULL) {
        as->mytable = (struct PTE*) kmalloc(sizeof (struct PTE));
        if (as->mytable == NULL) {
            return ENOMEM;
        }

        itr = as->mytable;
        as->mytable->paddr = getppages(1, 0, as->mytable);
        if (as->mytable->paddr == NULL) {
            return ENOMEM;
        }

        as->mytable->vaddr = as->region1;
        as->mytable->next = NULL;
        as->mytable->swapped = 0;
        as->mytable->slot_disk = -1;

        int i, k, h;
        for (i = 1; i < as->npage_region1; i++) {

            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }

            new_PTE->paddr = getppages(1, 0, new_PTE);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->vaddr = as->region1 + PAGE_SIZE*i;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;

            itr->next = new_PTE;
            itr = new_PTE;
        }
        for (k = 0; k < as->npage_region2; k++) {

            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }
            new_PTE->paddr = getppages(1, 0, new_PTE);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->vaddr = as->region2 + PAGE_SIZE*k;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;
            itr->next = new_PTE;
            itr = new_PTE;

        }
        /*getting pages for stack*/
        /*
        for (h = 0; h < STACKPAGES; h++) {

            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }
            new_PTE->paddr = getppages(1,0, new_PTE);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->vaddr = USERSTACK - PAGE_SIZE*h;
            new_PTE->next = NULL;
            itr->next = new_PTE;
            itr = new_PTE;

        }
         */

    } else {

        while (itr->next != NULL)
            itr = itr->next;

        int j = 0, a = 0, b = 0;
        for (j = 0; j < as->npage_region1; j++) {
            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }

            new_PTE->paddr = getppages(1, 0, new_PTE);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->next = NULL;
            new_PTE->vaddr = as->region1 + PAGE_SIZE*j;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;

            itr->next = new_PTE;
            itr = new_PTE;
        }
        for (a = 0; a < as->npage_region2; a++) {

            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }
            new_PTE->paddr = getppages(1, 0, new_PTE);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->vaddr = as->region2 + PAGE_SIZE*a;
            new_PTE->next = NULL;
            new_PTE->swapped = 0;
            new_PTE->slot_disk = -1;

            itr->next = new_PTE;
            itr = new_PTE;

        }
        /*getting pages for stack*/
        /*
        for (b = 0; b < STACKPAGES; b++) {

            new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
            if (new_PTE == NULL) {
                return ENOMEM;
            }
            new_PTE->paddr = getppages(1,0);
            if (new_PTE->paddr == NULL) {
                return ENOMEM;
            }

            new_PTE->vaddr = USERSTACK - PAGE_SIZE*b;
            new_PTE->next = NULL;
            itr->next = new_PTE;
            itr = new_PTE;

        }
         */




    }




    return 0;
}

int
as_complete_load(struct addrspace * as) {
    (void) as;
    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t * stackptr) {
    //assert(as->as_stackpbase != 0);

    *stackptr = USERSTACK;
    return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret) {



    struct addrspace *new;

    new = as_create();
    if (new == NULL) {
        return ENOMEM;
    }

    new->region1 = old->region1;
    new->npage_region1 = old->npage_region1;
    new->region2 = old->region2;
    new->npage_region2 = old->npage_region2;
    new->heap_start = old->heap_start;
    new->heap_break = old->heap_break;
    new->heap_end = old->heap_end;
    new->heap_npage = old->heap_npage;
    new->page_not_filled = old->page_not_filled;

    new->memsize1 = old->memsize1;
    new->memsize2 = old->memsize2;
    new->filesize1 = old->filesize1;
    new->filesize2 = old->filesize2;

    new->executable1 = old->executable1;
    new->executable2 = old->executable2;
    new->offset1 = old->offset1;
    new->offset2 = old->offset2;
    new->v = old->v;

    /*
    if (as_prepare_load(new)) {
        as_destroy(new);
        return ENOMEM;
    }
    
    struct PTE *new_PTE;
     */


    //now copying the linked list page table
    //new address space must be NULL

    assert(new->mytable == NULL);

    //new->mytable = (struct PTE*) kmalloc(sizeof (struct PTE));




    struct PTE *old_PTE;
    struct PTE *itr;
    struct PTE *new_PTE;
    new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));

    if (new_PTE == NULL) {
        return ENOMEM;
    }

    //itr = as->mytable;

    //dont have to malloc old_PTE since mytable already got malloced
    old_PTE = old->mytable;
    new->mytable = new_PTE;
    new_PTE->vaddr = old_PTE->vaddr;
    new_PTE->paddr = getppages(1, 0, new_PTE);
    new_PTE->next = NULL;
    new_PTE->swapped = 0;
    new_PTE->slot_disk = -1;

    itr = new_PTE;
    old_PTE = old_PTE->next;

    //traverse the old 
    while (old_PTE != NULL) {

        //now new_PTE get malloc
        new_PTE = (struct PTE*) kmalloc(sizeof (struct PTE));
        new_PTE->vaddr = old_PTE->vaddr;
        if (old_PTE->swapped == 0) {
            new_PTE->paddr = getppages(1, 0, new_PTE);
        }
        new_PTE->next = NULL;
        new_PTE->swapped = 0;
        new_PTE->slot_disk = -1;

        itr->next = new_PTE;
        itr = new_PTE;
        old_PTE = old_PTE->next;
    }

    //assert(new->as_stackpbase != 0);

    struct PTE* old2_PTE = old->mytable;
    new_PTE = new->mytable;
    while (new_PTE != NULL) {
        if (old2_PTE->swapped == 0) {
            memmove((void *) PADDR_TO_KVADDR(new_PTE->paddr),
                    (const void *) PADDR_TO_KVADDR(old2_PTE->paddr),
                    PAGE_SIZE);
        } else if (old2_PTE->swapped == 1) {
            paddr_t paddr = getppages(1, 0, new_PTE);

            struct uio myuio;

            mk_kuio(&myuio, PADDR_TO_KVADDR(paddr), PAGE_SIZE, (old2_PTE->slot_disk) * PAGE_SIZE, UIO_READ);

            mk_kuio(&myuio, PADDR_TO_KVADDR(paddr), PAGE_SIZE, PAGE_SIZE*slot_count, UIO_WRITE);
            
            new_PTE->swapped = 1;
            
            new_PTE->slot_disk = slot_count;

            slot_count++;

        }

        new_PTE = new_PTE->next;
        old2_PTE = old2_PTE->next;
    }


    *ret = new;
    return 0;


}

int load_page_segment(struct vnode *v, off_t offset, vaddr_t vaddr,
        size_t pmemsize, size_t pfilesize,
        int is_executable) {

    struct uio u;
    int result;
    size_t fillamt;




    u.uio_iovec.iov_ubase = (userptr_t) (vaddr);
    u.uio_iovec.iov_len = pmemsize; // length of the memory space
    u.uio_resid = pfilesize; // amount to actually read
    u.uio_offset = offset;
    u.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = curthread->t_vmspace;

    result = VOP_READ(v, &u);
    if (result) {
        return result;
    }

    if (u.uio_resid != 0) {
        /* short read; problem with executable? */
        kprintf("ELF: short read on segment - file truncated?\n");
        return ENOEXEC;
    }

    /* Fill the rest of the memory space (if any) with zeros */
    fillamt = pmemsize - pfilesize;
    if (fillamt > 0) {
        //    kprintf("fill zero\n");
        DEBUG(DB_EXEC, "ELF: Zero-filling %lu more bytes\n",
                (unsigned long) fillamt);
        u.uio_resid += fillamt;
        result = uiomovezeros(fillamt, &u);
        if (result) {
            return result;
        }
    }

    return 0;
}
