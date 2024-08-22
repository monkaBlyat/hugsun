#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/version.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
//#include <../include/net/irda/wrapper.h>
#include "../../../include/config/arch/has/syscall/wrapper.h"

#include <linux/ioctl.h>

#ifndef CONFIG_PCI
#  error "This driver needs PCI support to be available"
#endif
#include "memMgr.h"

/* device open */
int memMgrdrv_open(struct inode *inode, struct file *file);
/* device close */
int memMgrdrv_release(struct inode *inode, struct file *file);
/* device mmap */
int memMgrdrv_mmap(struct file *file, struct vm_area_struct *vma);

/* the ordinary device operations */
static struct file_operations memMgrdrv_fops =
{
  owner:   THIS_MODULE,
  mmap:    memMgrdrv_mmap,
  open:    memMgrdrv_open,
  release: memMgrdrv_release,
};

static int memMgrAlreadyOpen = 0;
static unsigned long  *virtMemTable[K_NB_BLOCK];

/* pointer to page aligned area */
static  unsigned long *physMemTable_area = (void *)NULL;
static void  *kmalloc_area = NULL;

/* pointer to unaligned area */
static unsigned long  *physMemTable_ptr = NULL;

/* major number of device */
static int major;


static void memMgrFreeMem(void)
{
   int i, count;
   unsigned long virt_addr;

   /* and free the two areas */
   if (physMemTable_ptr)
   {
	count=0;
      	for(i=0;i<K_NB_BLOCK;i++)
      	{
        	if (virtMemTable[i] != NULL) 
         	{
           	/* unreserve all pages */
           		for(virt_addr=(unsigned long)virtMemTable[i]; 
               		virt_addr<(unsigned long)virtMemTable[i]+K_BLOCK_SIZE;
               		virt_addr+=PAGE_SIZE)
           		{
                		ClearPageReserved(virt_to_page(virt_addr));
				count++;
           		}
 		         printk("free virtMemTable[%d] =  %p \n",i,virtMemTable[i] );

	   		kfree(virtMemTable[i]);
           		virtMemTable[i] = NULL;
         	}
      	}
        printk(" Number of freed pages %d\n", count);
      	count=0;
      	for(virt_addr=(unsigned long)physMemTable_ptr;
            virt_addr<(unsigned long)physMemTable_ptr+K_NB_BLOCK;
            virt_addr+=PAGE_SIZE)
        {
        	ClearPageReserved(virt_to_page(virt_addr));
          	count++;
     	}

     	printk("free physMemTable_ptr   %p \n",physMemTable_ptr );

  	kfree (physMemTable_ptr);
    	printk(" Number of freed pages %d\n", count);

   }
}

/* load the module */
static int __init memMgr_init_module(void)
{
   int i,j, count;
   unsigned long virt_addr;
        
   /* Get the base adress of physical allocated adresses with kmalloc and aligned 
      it to a page. This area will be physically contigous */
   physMemTable_ptr=kmalloc(K_NB_BLOCK*sizeof(unsigned long)+2*PAGE_SIZE, GFP_KERNEL);
   count=0;
   if( physMemTable_ptr != NULL)
   {
//   	printk("physMemTable_ptr %p \n", physMemTable_ptr);
	physMemTable_area=(void  *)(((unsigned long)physMemTable_ptr + PAGE_SIZE -1) & PAGE_MASK);
   	for (virt_addr=(unsigned long)physMemTable_area; 
       	     virt_addr<(unsigned long)physMemTable_area+K_NB_BLOCK*sizeof(unsigned long);
             virt_addr+=PAGE_SIZE)
   	{
      /* reserve all pages to make them remapable */
      		SetPageReserved(virt_to_page(virt_addr));
	     	count++;
                 
	}
//        printk(" Number of pages %d\n", count);
   }
   else
   {
	return(-1);
   }

   /* Get kernel memory, this will be physically contigous and aligned to PAGE_SIZE */
   for(i=0;i<K_NB_BLOCK;i++)
   {
	virtMemTable[i] = kmalloc(K_BLOCK_SIZE+2*PAGE_SIZE,GFP_KERNEL );

      	if (virtMemTable[i] == NULL) 
      	{
		printk("<1>memMgr_initmodule : unable to allocate memory %d\n",i);
		memMgrFreeMem();

	        return (-1);
      	}
      	else
      	{

//     		printk("virtMemTable[%d] =  %p \n",i,virtMemTable[i] );

        	kmalloc_area = (void *)(((unsigned long)virtMemTable[i] + PAGE_SIZE -1) & PAGE_MASK);

//          	physMemTable_area[i] = virt_to_phys(virtMemTable[i]);
                physMemTable_area[i] = virtMemTable[i];
  
                printk("physMemTable_area[%d] =  %p \n",i,physMemTable_ptr[i] );
      
      	}
  	count=0;
      	for (virt_addr=(unsigned long)kmalloc_area ; 
             virt_addr<(unsigned long)kmalloc_area + K_BLOCK_SIZE;
             virt_addr+=PAGE_SIZE)
      	{
         /* reserve all pages to make them remapable */
       		SetPageReserved(virt_to_page(virt_addr));
        	count++;
	}
//      	printk(" Number of reserved pages %d\n", count);
       // Initialize memory
       for (j=0; j<(K_BLOCK_SIZE/sizeof(int)); j+=2)
       {
         /* initialise with some dummy values to compare later */
       		((int *)virtMemTable[i])[j]=(0xAA55) ;
         	((int *)virtMemTable[i])[j+1]=(0xAA55);
       }
   }
//   printk("Value = %x\n", ((int *)virtMemTable[0])[0]);
   if ((major=register_chrdev(0, "memMgrdrv", &memMgrdrv_fops))<0) 
   {
  	printk("<1>memMgrdrv: unable to register character device\n");
     	memMgrFreeMem();
      	return (-EIO);
   }


   return(0);
}

/* remove the module */
static void __exit memMgr_cleanup_module(void)
{
    memMgrFreeMem();
    /* unregister the device */
    unregister_chrdev(major, "memMgrdrv");
    return;
}

/* device open method */
int memMgrdrv_open(struct inode *inode, struct file *file)
{
    if(memMgrAlreadyOpen)
    {
    	printk("<1>This device can be opened only once\n");
        return(-EIO);
    }

    memMgrAlreadyOpen = 1;
    return(0);
}

/* device close method */
int memMgrdrv_release(struct inode *inode, struct file *file)
{
    memMgrAlreadyOpen = 0;
    return(0);
}

/* device memory map method */
int memMgrdrv_mmap(struct file *file, struct vm_area_struct *vma)
{
    int ret;
    unsigned long offset = vma->vm_pgoff<<PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size> K_BLOCK_SIZE)
    {
    	printk("memMgrdrv_mmap : size too big %ld %ld\n",size, K_BLOCK_SIZE);
	return(-ENXIO);
    }

    if (offset & ~PAGE_MASK)
    {
    	printk("memMgrdrv_mmap : offset not aligned: %ld\n", offset);
        return -ENXIO;
    }
        
        /* we only support shared mappings. Copy on write mappings are
           rejected here. A shared mapping that is writeable must have the
           shared flag set.
        */
    if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED))
    {
   	printk("memMgrdrv_mmap : writeable mappings must be shared, rejecting\n");
        return(-EINVAL);
    }

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

        /* Don't try to swap out physical pages.. */
    vma->vm_flags |= VM_IO;

        /*
         * Don't dump addresses that are not real memory to a core file.
         */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,6,99)
      vma->vm_flags |= VM_RESERVED;
#else
      vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;  //for kernel > 3.2
#endif
  
        /*  we do not want to have this area swapped out, lock it */
    vma->vm_flags |= VM_LOCKED;
    if(offset ==0)
    {       
    	ret = remap_pfn_range(vma, vma->vm_start,
                              virt_to_phys((void*)(physMemTable_area))>> PAGE_SHIFT,
             	              vma->vm_end-vma->vm_start,
               		      PAGE_SHARED);

        if(ret != 0)
	{
               	return -ENXIO;
        }
    }
    else
    {
        printk("start %x offset %x\n",vma->vm_start, offset);

        if (remap_pfn_range(vma, vma->vm_start,
                            virt_to_phys(offset)>>PAGE_SHIFT,
                            vma->vm_end-vma->vm_start,
                            PAGE_SHARED))
        {
                printk("memMgrdrv_mmap : remap page range failed\n");
                return -ENXIO;
        }
     }
     return(0);
}
                                     
module_exit(memMgr_cleanup_module);
module_init(memMgr_init_module);
MODULE_LICENSE("GPL");
