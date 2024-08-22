#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include "memMgr.h"

/* File Descriptor of /dev/memMgr */
static int memMgrFd = 0;

/* memBlock In Use table */
static int memBlockInUse[K_NB_BLOCK];
/* virtual Addresss table */
static unsigned long *virtMemTable[K_NB_BLOCK];
/* pointer to physical block table */
static unsigned long  *physMemTable = NULL;

static pthread_mutex_t memMgrMutex = PTHREAD_MUTEX_INITIALIZER;

int memMgrInit(void)
{
   int i;

   if (physMemTable != NULL)
   {
       return(-1);
   }

   /* Open the device */
   if ((memMgrFd = open("/dev/memMgr", O_RDWR))<0)
   {
      perror("Open /dev/memMgr");
      return(-1);
   }

   /* Map the Physical Adresses Table */
   physMemTable = mmap(0, K_NB_BLOCK*sizeof(unsigned long), PROT_WRITE |  PROT_READ, MAP_SHARED, memMgrFd, 0);
  
   if (physMemTable == MAP_FAILED)
   {
      perror("memMgrInit");
      return(-1);
   }
   else
   {
   	printf("PhysMemTable %p\n", physMemTable);
   
   }

   /* map all blocks */

   for(i=0;i<K_NB_BLOCK;i++)
   {
//      printf(" address mapped %lx\n", (unsigned long)physMemTable[i]);
      memBlockInUse[i] = 0;
      virtMemTable[i] = mmap(0, K_BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, 
                                    memMgrFd, (unsigned long)physMemTable[i]);
      if (virtMemTable[i] == MAP_FAILED)
      {
	  printf("Map Failed\n");
          perror("memMgrInit : mmap");
          memBlockInUse[i] = 1;
          virtMemTable[i] = NULL;
	  return(-1);
      }
//      printf("PhysMemTable %p %p %x\n", physMemTable[i],virtMemTable[i],((int *)virtMemTable[i]));

   }

   return(0);
}


int memMgrClose(void)
{
   int localStatus = 0;
   int i;

   /* Free all blocks */
   for(i=0;i<K_NB_BLOCK;i++)
   {
      if (virtMemTable[i] != NULL)
      {
          localStatus = munmap(virtMemTable[i] , K_BLOCK_SIZE);
          if (localStatus != 0)
          {
             perror("memMgr munmap");
             return(-1);
          }
//          else printf(" Success unallocated %p\n", virtMemTable[i]);
      }
   }

   /* Unmap the Physical Adresses Table */
   localStatus = munmap(physMemTable, K_NB_BLOCK*sizeof(unsigned long));
   
   if (localStatus != 0)
   {
      perror("memMgr munmap");
      return(-1);
   }
   else printf(" Success allocated %p\n", physMemTable);
   /* close the device */
   close(memMgrFd);

   return(0);
}


int memMgrAllocateBlock(void ** logicalAddress, unsigned long * physicalAddress)
{
   int i;
   int memBlockId = -1; 

   if(memMgrFd == 0)
   {
       return(-1);
   }

   pthread_mutex_lock(&memMgrMutex);

   /* Search a freeBlock */
   for(i=0;i<K_NB_BLOCK;i++)
   {
      if(memBlockInUse[i] == 0)
      {
         *physicalAddress = physMemTable[i];
         *logicalAddress = virtMemTable[i];
         memBlockId = i;
         memBlockInUse[i] = 1;
         break;
      }
   }
    
   pthread_mutex_unlock(&memMgrMutex);

   return(memBlockId);
}


void memMgrFreeBlock(int memBlockId)
{
   if(memMgrFd == 0)
   {
       return;
   }

   if((memBlockId < 0) || (memBlockId>K_NB_BLOCK))
      return;

   pthread_mutex_lock(&memMgrMutex);

   memBlockInUse[memBlockId] = 0;

   pthread_mutex_unlock(&memMgrMutex);
}

