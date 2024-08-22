
#define __MEM_MGR_H__


#define K_NB_BLOCK      12 
#define K_BLOCK_SIZE    (4*1024*1024 - 2*PAGE_SIZE)


#ifdef __cplusplus
extern "C" {
#endif

int  memMgrInit(void);
int  memMgrClose(void);
int  memMgrAllocateBlock(void ** logicalAddress, unsigned long  * physicalAddress);
void memMgrFreeBlock(int memBlockId);

#ifdef __cplusplus
}
#endif


