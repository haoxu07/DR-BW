/*****************************************************************************
* This example shows how to use PAPI_overflow to set up an event set to      *
* begin registering overflows.
******************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "monitor.h"
#include "monitor_ext.h"
#include <pthread.h>
#include <unistd.h>
//#include <atomic.h>
#include <sys/syscall.h>
//#include <sys/shm.h>
#include <sys/mman.h>

#include <numa.h>
#include <numaif.h>

//needed to reload the malloc functions
#include <sys/types.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include "rdtsc.h"

//needed to handle timer interrupt
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <execinfo.h>
#include "backtrace-symbol.h"

#if PEBS
#include "papi.h"
#elif IBS
#include <signal.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_amd64.h>
 
#include <perfmon/perfmon.h>
#include <perfmon/perfmon_dfl_smpl.h>
#include <fcntl.h>
#include <sched.h>
#endif

#if PEBS
#if PEBS_POW
#define THRESHOLD 10
#else
//#define THRESHOLD 100//000 
#define THRESHOLD_CYC 10000000
#endif
#define ERROR_RETURN(retval) { fprintf(stderr, "Error %d %s:line %d: \n", retval,__FILE__,__LINE__);  exit(retval); }
#endif

/* type of opcode (load/store/prefetch,code) */
#define PERF_MEM_OP_NA          0x01 /* not available */
#define PERF_MEM_OP_LOAD        0x02 /* load instruction */
#define PERF_MEM_OP_STORE       0x04 /* store instruction */
#define PERF_MEM_OP_PFETCH      0x08 /* prefetch */
#define PERF_MEM_OP_EXEC        0x10 /* code (execution) */
/* memory hierarchy (memory level, hit or miss) */
#define PERF_MEM_LVL_NA         0x01  /* not available */
#define PERF_MEM_LVL_HIT        0x02  /* hit level */
#define PERF_MEM_LVL_MISS       0x04  /* miss level  */
#define PERF_MEM_LVL_L1         0x08  /* L1 */
#define PERF_MEM_LVL_LFB        0x10  /* Line Fill Buffer */
#define PERF_MEM_LVL_L2         0x20  /* L2 hit */
#define PERF_MEM_LVL_L3         0x40  /* L3 hit */
#define PERF_MEM_LVL_LOC_RAM    0x80  /* Local DRAM */
#define PERF_MEM_LVL_REM_RAM1   0x100 /* Remote DRAM (1 hop) */
#define PERF_MEM_LVL_REM_RAM2   0x200 /* Remote DRAM (2 hops) */
#define PERF_MEM_LVL_REM_CCE1   0x400 /* Remote Cache (1 hop) */
#define PERF_MEM_LVL_REM_CCE2   0x800 /* Remote Cache (2 hops) */
#define PERF_MEM_LVL_IO         0x1000 /* I/O memory */
#define PERF_MEM_LVL_UNC        0x2000 /* Uncached memory */
/* snoop mode */
#define PERF_MEM_SNOOP_NA       0x01 /* not available */
#define PERF_MEM_SNOOP_NONE     0x02 /* no snoop */
#define PERF_MEM_SNOOP_HIT      0x04 /* snoop hit */
#define PERF_MEM_SNOOP_MISS     0x08 /* snoop miss */
#define PERF_MEM_SNOOP_HITM     0x10 /* snoop hit modified */
/* locked instruction */
#define PERF_MEM_LOCK_NA        0x01 /* not available */
#define PERF_MEM_LOCK_LOCKED    0x02 /* locked transaction */
/* TLB access */
#define PERF_MEM_TLB_NA         0x01 /* not available */
#define PERF_MEM_TLB_HIT        0x02 /* hit level */
#define PERF_MEM_TLB_MISS       0x04 /* miss level */
#define PERF_MEM_TLB_L1         0x08 /* L1 */
#define PERF_MEM_TLB_L2         0x10 /* L2 */
#define PERF_MEM_TLB_WK         0x20 /* Hardware Walker*/
#define PERF_MEM_TLB_OS         0x40 /* OS fault handler */

/* infrastructure for shadow memory */
/* MACROs */
// 64KB shadow pages
#define PAGE_OFFSET_BITS (16LL)
#define PAGE_OFFSET(addr) ( addr & 0xFFFF)
#define PAGE_OFFSET_MASK ( 0xFFFF)

#define PAGE_SIZE (1 << PAGE_OFFSET_BITS)

// 2 level page table
#define PTR_SIZE (sizeof(struct Status *))
#define LEVEL_1_PAGE_TABLE_BITS  (20)
#define LEVEL_1_PAGE_TABLE_ENTRIES  (1 << LEVEL_1_PAGE_TABLE_BITS )
#define LEVEL_1_PAGE_TABLE_SIZE  (LEVEL_1_PAGE_TABLE_ENTRIES * PTR_SIZE )

#define LEVEL_2_PAGE_TABLE_BITS  (12)
#define LEVEL_2_PAGE_TABLE_ENTRIES  (1 << LEVEL_2_PAGE_TABLE_BITS )
#define LEVEL_2_PAGE_TABLE_SIZE  (LEVEL_2_PAGE_TABLE_ENTRIES * PTR_SIZE )

#define LEVEL_1_PAGE_TABLE_SLOT(addr) (((addr) >> (LEVEL_2_PAGE_TABLE_BITS + PAGE_OFFSET_BITS)) & 0xfffff)
#define LEVEL_2_PAGE_TABLE_SLOT(addr) (((addr) >> (PAGE_OFFSET_BITS)) & 0xFFF)

#define MAX_NODES 8
#define MAX_CPU 1024
#define MAX_THREADS 512
#define MAX_MALLOC 2048*16
#define TIME_THRESHOLD 1000 //timer interval for interrupt
#define MAX_LATENCY 15
#define SAMPLE_THRESHOLD 4

#define NDIM 2000
#define MAX_FD 1024

// Sampling frequency:  one sample from every THRESHOLD memory accesses
int THRESHOLD = 2000;

typedef void *malloc_fcn_t(size_t);
typedef void *mmap_fcn_t(void *, size_t, int , int , int, off_t);
typedef void *calloc_fcn_t(size_t, size_t);
typedef void *realloc_fcn_t(void *, size_t);
typedef void *memalign_fcn_t(size_t, size_t);
typedef void *valloc_fcn_t(size_t);
typedef int  posix_memalign_fcn_t(void**, size_t, size_t);
typedef void free_fcn_t(void *);


#ifdef MONITOR_STATIC
extern malloc_fcn_t __real_malloc;
extern free_fcn_t __real_free;
extern calloc_fcn_t __real_calloc;
extern realloc_fcn_t __real_realloc;
extern memalign_fcn_t __real_memalign;
extern valloc_fcn_t __real_valloc;
extern posix_memalign_fcn_t __real_posix_memalign;
extern mmap_fcn_t __real_mmap;
#endif

#define real_memalign   __libc_memalign
#define real_valloc   __libc_valloc
#define real_malloc   __libc_malloc
#define real_free     __libc_free
#define real_realloc  __libc_realloc
#define real_calloc  __libc_calloc
//#define real_mmap __mman_mmap

extern malloc_fcn_t real_malloc;
extern free_fcn_t real_free;
extern realloc_fcn_t real_realloc;
extern memalign_fcn_t real_memalign;
extern valloc_fcn_t real_valloc;
extern calloc_fcn_t real_calloc;
extern mmap_fcn_t real_mmap;

static uint8_t ** gL1PageTable[LEVEL_1_PAGE_TABLE_SIZE];

struct ThreadAffinity{

    int threadL;
    int threadR;
    int count;

}; 

__thread int mallocFlag = 0;
__thread int totalSample = 0;
// Avoid for recursive calling print_stack when to backtrace(Because backtrace will call malloc every time)
__thread int backtraceFlag = 1;
//used in thread migration
__thread int threadid = 0;
__thread uint64_t L1Counter=0;

int Nodes;
int CpusperNode;

//used on malloc profiling
__thread uint64_t mallocID = 0;
__thread uint64_t mallocIdx = 0;
uint64_t SIZE_TO_PROFILE =4096*256;
uint64_t mallocCnt[MAX_THREADS];
uint64_t mallocSumm[MAX_THREADS][MAX_MALLOC][3];
int mallocInter[MAX_THREADS][MAX_MALLOC][3];
int PROFILE_FLAG = 1;
__thread int PRINT_STACK_FLAG = 1;
__thread int SamNumber = 0;
int flag = 0;

#ifdef PEBS
int isPapiInitialized = 0;

__thread int PAPI_event;
__thread int EventSet = PAPI_NULL;
__thread long long L3DCM;
//long long L3DCM_r;
//long long L3DCM_p;

//record samples to buffer, when it is full, save to files:
  struct Records{
     char str[150];
     struct Records *next;
  };

  struct stack_Records{
     char str[4000];
     struct stack_Records *next;
  };

  struct mallocInformation{
	uint64_t size;
	int MallocID;
	int threadID;
	uint64_t addr; 
  };


__thread struct Records *head = NULL;
__thread struct Records *curr = NULL;

__thread struct stack_Records *stack_head = NULL;
__thread struct stack_Records *stack_curr = NULL;

typedef union perf_mem_data_src {
        uint64_t val;
        struct {
                uint64_t   mem_op:5,       /* type of opcode */
                        mem_lvl:14,     /* memory hierarchy level */
                        mem_snoop:5,    /* snoop mode */
                        mem_lock:2,     /* lock instr */
                        mem_dtlb:7,     /* tlb access */
                        mem_rsvd:31;
        };
}perf_mem_data_src;

pthread_mutex_t idLock;
int threadID = 4; /////////////////number of threads
int NUM_CPU = 0;

#endif

#ifdef IBS

pthread_mutex_t idLock;
int threadID = 0;
int NUM_CPU = 0;

typedef pfm_dfl_smpl_arg_t              smpl_fmt_arg_t;
typedef pfm_dfl_smpl_hdr_t              smpl_hdr_t;
typedef pfm_dfl_smpl_entry_t            smpl_entry_t;
typedef pfm_dfl_smpl_arg_t              smpl_arg_t;

size_t entry_size;
#define BPL (sizeof(uint64_t)<<3)
#define LBPL    6

static inline void pfm_bv_set(uint64_t *bv, uint16_t rnum)
{
        bv[rnum>>LBPL] |= 1UL << (rnum&(BPL-1));
}

static inline int pfm_bv_isset(uint64_t *bv, uint16_t rnum)
{
        return bv[rnum>>LBPL] & (1UL <<(rnum&(BPL-1))) ? 1 : 0;
}

struct over_args {
        pfmlib_amd64_input_param_t inp_mod;
        pfmlib_output_param_t outp;
        pfmlib_amd64_output_param_t outp_mod;
        pfarg_pmc_t pc[PFMLIB_MAX_PMCS];
        pfarg_pmd_t pd[PFMLIB_MAX_PMDS];
        pfarg_ctx_t ctx;
        pfarg_load_t load_args;
        int fd;
        pid_t tid;
        pthread_t self;
        int count;
	smpl_hdr_t *hdr;
};
 
struct over_args fd2ov[MAX_FD];

#endif

pid_t
gettid(void)
{
        return (pid_t)syscall(__NR_gettid);
}

unsigned Num_migration = 0;

int cpu_node_map[MAX_CPU];

//Create linkedlist for recording the sampled memory access information
struct Records* create_list(){

    struct Records *ptr = (struct Records*)real_malloc(sizeof(struct Records));
    if(ptr == NULL){
       printf("Node creation failed\n");
       exit(0);
    }
    sprintf(ptr->str,"timestamp,cpu,thread,numaaccess,numalocation,ip,dataAddr,memOP,memSize,memHR,latency\n");
    ptr->next = NULL;

    head = curr = ptr;
    return ptr;
}

// Create linkedlist for recording the calling context
struct stack_Records* create_list_stack(){
        
    struct stack_Records *stack_ptr = (struct stack_Records*)real_malloc(sizeof(struct stack_Records));
    if(stack_ptr == NULL){
       printf("Node creation failed\n");
       exit(0);
    }
    stack_ptr->next = NULL;
    sprintf(stack_ptr->str,"malloc ID, thread ID\n ...\n");
    stack_head = stack_curr = stack_ptr;
    return stack_ptr;
}
__thread int print_stack_cnt = 0;

// Print the stack of the calling context and record it in a linkedlist
void print_stack(int threadid, uint64_t mallocIdx, void *ptr, size_t size)  
{  
    int backtrace_size = 64;
    mallocCnt[threadid] = mallocIdx; 
    print_stack_cnt++; 
    int i;    
    void *array[64];  
    int stack_num = backtrace(array, backtrace_size);  
    char **stacktrace = NULL;  
     
    stacktrace = (char**)backtrace_symbols(array, stack_num); 
    mallocFlag=1;
    struct stack_Records *stack_ptr = (struct stack_Records *)real_malloc(sizeof(struct stack_Records));
    mallocFlag=0;
    if( stack_ptr == NULL) ERROR_RETURN(1);
    sprintf(stack_ptr->str,"mallocID: %ld thread ID:%d  ptr:%p size:%ld after stack\n", mallocIdx, threadid, ptr, size);
    for(i=0;i<stack_num;i++){
       sprintf(stack_ptr->str,"%s%s\n",stack_ptr->str,stacktrace[i]);
    }
    
    stack_ptr->next = NULL;    
    if(stack_curr == NULL){	
	printf("Create list %d\n", print_stack_cnt);
	create_list_stack();
    }
    stack_curr->next = stack_ptr;
    stack_curr = stack_ptr;
    free(stacktrace);
}  

// Wrapper of malloc to record every malloc
void *MONITOR_EXT_WRAP_NAME(malloc)(size_t size){
    
    mallocFlag = 1;
    if(PROFILE_FLAG){
       void *ptr = real_malloc(size);
       if((int64_t)size >= SIZE_TO_PROFILE && mallocIdx<MAX_MALLOC && PRINT_STACK_FLAG == 1){  
	  mallocSumm[threadid][mallocIdx][0] = mallocID;
          mallocSumm[threadid][mallocIdx][1] = (uint64_t)ptr;
          mallocSumm[threadid][mallocIdx][2] = (uint64_t)size; 
	  PRINT_STACK_FLAG = 0;
	  print_stack(threadid, mallocIdx, ptr, size);
          PRINT_STACK_FLAG = 1;
          mallocIdx++;
       }
       mallocID++;
       mallocFlag = 0;
       return ptr;
    }else{

       void *ptr = real_malloc(size);
       mallocFlag = 0;
       return ptr;
    }
}

// Wrapper of free
void MONITOR_EXT_WRAP_NAME(free)(void *ptr){

     mallocFlag = 1;
     real_free(ptr);
     mallocFlag = 0;
}

// Wrapper of calloc to record every calloc
void *MONITOR_EXT_WRAP_NAME(calloc)(size_t nmemb, size_t size){

    mallocFlag = 1;
    if(PROFILE_FLAG){
       void *ptr = (*real_calloc)(nmemb,size);
       if(nmemb*size >= SIZE_TO_PROFILE && mallocIdx<MAX_MALLOC && PRINT_STACK_FLAG == 1){ 
	  mallocSumm[threadid][mallocIdx][0] = mallocID;
          mallocSumm[threadid][mallocIdx][1] = (uint64_t)ptr;
          mallocSumm[threadid][mallocIdx][2] = (uint64_t)nmemb*size; 
	  PRINT_STACK_FLAG = 0;
	  print_stack(threadid, mallocIdx, ptr, nmemb*size);
	  PRINT_STACK_FLAG = 1;
	  mallocIdx++;
       }
       mallocID++;
       mallocFlag = 0;
       return ptr;
    }else{
       void *ptr = (*real_calloc)(nmemb,size);
       mallocFlag = 0;
       return ptr;

    } 
}

// Wrapper of realloc to record every realloc
void *MONITOR_EXT_WRAP_NAME(realloc)(void *ptr, size_t size){
 
    mallocFlag = 1;
    if(PROFILE_FLAG){
       void *nptr = real_realloc(ptr,size);
       if((int64_t)size >= SIZE_TO_PROFILE && mallocIdx<MAX_MALLOC && PRINT_STACK_FLAG == 1){ 
	  mallocSumm[threadid][mallocIdx][0] = mallocID;
          mallocSumm[threadid][mallocIdx][1] = (uint64_t)nptr;
          mallocSumm[threadid][mallocIdx][2] = (uint64_t)size; 
	  PRINT_STACK_FLAG = 0;
	  print_stack(threadid, mallocIdx, nptr, size);
          PRINT_STACK_FLAG = 1;
	  mallocIdx++;
       }
       mallocID++;
       mallocFlag = 0;
       return nptr;
    }else{

       void *nptr = real_realloc(ptr,size);
       mallocFlag = 0;
       return nptr;
    }   
}

// Wrapper of memalign to record every memalign
void *MONITOR_EXT_WRAP_NAME(memalign)(size_t blocksize, size_t bytes){
 
    mallocFlag = 1;
    if(PROFILE_FLAG){
       void *ptr = (real_memalign)(blocksize, bytes);
       if((int64_t)blocksize >= SIZE_TO_PROFILE && mallocIdx<MAX_MALLOC && PRINT_STACK_FLAG == 1){
          mallocSumm[threadid][mallocIdx][0] = mallocID;
          mallocSumm[threadid][mallocIdx][1] = (uint64_t)ptr;
          mallocSumm[threadid][mallocIdx][2] = (uint64_t)blocksize;
	  PRINT_STACK_FLAG = 0;
	  print_stack(threadid, mallocIdx, ptr, blocksize);
	  PRINT_STACK_FLAG = 1;
	  mallocIdx++; 
       }
       mallocID++;
       mallocFlag = 0;
       return ptr;
    }else{
       void *ptr = (real_memalign)(blocksize, bytes);
       mallocFlag = 0;
       return ptr;

    } 
}

// Wrapper of valloc to record every valloc
void *MONITOR_EXT_WRAP_NAME(valloc)(size_t size){
 
    mallocFlag = 1;
    if(PROFILE_FLAG){
       void *ptr = (real_valloc)(size);
       if((int64_t)size >= SIZE_TO_PROFILE && mallocIdx<MAX_MALLOC && PRINT_STACK_FLAG == 1){ 
	  mallocSumm[threadid][mallocIdx][0] = mallocID;
          mallocSumm[threadid][mallocIdx][1] = (uint64_t)ptr;
          mallocSumm[threadid][mallocIdx][2] = (uint64_t)size; 
	  PRINT_STACK_FLAG = 0;
	  print_stack(threadid, mallocIdx, ptr, size);
	  PRINT_STACK_FLAG = 1;
	  mallocIdx++;
       }
       mallocID++;
       mallocFlag = 0;
       return ptr;
    }else{
       void *ptr = (real_valloc)(size);
       mallocFlag = 0;
       return ptr;
    } 
}


/* helper functions for shadow memory */
static uint8_t* GetOrCreateShadowBaseAddress(uint64_t address) {
    uint8_t *shadowPage;
    uint8_t ***l1Ptr = &gL1PageTable[LEVEL_1_PAGE_TABLE_SLOT(address)];
    if(*l1Ptr == 0) {
        *l1Ptr = (uint8_t **) calloc(1, LEVEL_2_PAGE_TABLE_SIZE);
        shadowPage = (*l1Ptr)[LEVEL_2_PAGE_TABLE_SLOT(address)] =  (uint8_t *) mmap(0, PAGE_SIZE * (sizeof(uint64_t)), PROT_WRITE | PROT_READ, MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    } else if((shadowPage = (*l1Ptr)[LEVEL_2_PAGE_TABLE_SLOT(address)]) == 0 ){
        shadowPage = (*l1Ptr)[LEVEL_2_PAGE_TABLE_SLOT(address)] =  (uint8_t *) mmap(0, PAGE_SIZE * (sizeof(uint64_t)), PROT_WRITE | PROT_READ, MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    }
    return shadowPage;
}

// Get the Local NOde the data access from
int getLocalNode(uint64_t ip){

    uint64_t addr = ip;
    uint8_t* status = GetOrCreateShadowBaseAddress(addr);
    uint64_t *prevAddr = (uint64_t *)(status + PAGE_OFFSET(addr) * sizeof(uint64_t));
    int localNode;
    void *data_addr = (void *)ip;

    if(*prevAddr == 0)
        move_pages(0, 1, &data_addr, NULL, &localNode, 0);
    else{

        int *accessValues;
        accessValues = (int *)(*prevAddr);
        if(accessValues[MAX_NODES] == -1)
           move_pages(0, 1, &data_addr, NULL, &localNode, 0);
        else 
           localNode = accessValues[MAX_NODES];
    }
    return localNode;
}

// Get which numa node the cpu belong
int getNumaNode(int cpu){

     if(cpu<0)
        printf("Error: Invalid cpu index\n");
     if(cpu_node_map[cpu] == -1)
        cpu_node_map[cpu] = numa_node_of_cpu(cpu);
     return cpu_node_map[cpu];
}

int setAffinity(int threadid){

  int cpu = threadid%NUM_CPU;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if(sched_setaffinity(gettid(), sizeof(cpuset), &cpuset) == -1){
           printf("set affinity failed\n");
  }
  return cpu;
}



struct mallocInformation* setMallocInter(uint64_t addr, int lat){

   int i,j;
   struct mallocInformation* mallocInfo= (struct mallocInformation*)real_malloc(sizeof(struct mallocInformation));
   mallocInfo->MallocID = -1;
   mallocInfo->threadID = -1;
   for(i=0; i<= threadID; i++){
      for(j=0;j<=mallocCnt[i];j++){
         if(mallocSumm[i][j][2] == 0)
            continue;
         if(mallocSumm[i][j][1] <= addr && addr < mallocSumm[i][j][1]+mallocSumm[i][j][2]){
  		mallocInfo->size = mallocSumm[i][j][2];
		mallocInfo->MallocID = j;
		mallocInfo->threadID = i;
		mallocInfo->addr = mallocSumm[i][j][1]; 
         }
      }
   }
   return mallocInfo;
}

// Using addr to find which data object the program want to access
int getMallocID(uint64_t addr, int lat){

   int i,j;
   for(i=0; i<= MAX_THREADS; i++){
      for(j=0;j<MAX_MALLOC;j++){
         if(mallocSumm[i][j][2] == 0)
            continue;
         if(mallocSumm[i][j][1] <= addr && addr < mallocSumm[i][j][1]+mallocSumm[i][j][2]){
           return j;
         }
      }
   }
   return -1;
}

// Get the maximum data object each program generated
int getMallocNum(int threadID){
   int i = 0;
   while(i<MAX_MALLOC && mallocSumm[threadID][i][2]!=0) i++;
   return i;
}

// Get the thread id of the data object belong
int getMallocThreadID(uint64_t addr, int lat){

   int i,j;
   for(i=0; i<= MAX_THREADS; i++){
      for(j=0;j<MAX_MALLOC;j++){
         if(mallocSumm[i][j][2] == 0)
            continue;
         if(mallocSumm[i][j][1] <= addr && addr < mallocSumm[i][j][1]+mallocSumm[i][j][2]){
           return i;
         }
      }
   }
   return -1;
}


#if IBS

void startIBS(int threadId)
{
  setAffinity(threadId);
  struct over_args *ov = &fd2ov[threadId];
  if (ov->fd) pfm_unload_context(ov->fd);
  memset(ov, 0, sizeof(struct over_args));

  int period=(65535<<4);
  ov->inp_mod.ibsop.maxcnt=period;
  ov->inp_mod.ibsop.options = IBS_OPTIONS_UOPS;
  ov->inp_mod.flags |= PFMLIB_AMD64_USE_IBSOP;
  int ret;
  ret=pfm_dispatch_events(NULL, &ov->inp_mod, &ov->outp, &ov->outp_mod);
  if (ret != PFMLIB_SUCCESS) {
    printf("dispatch events fails\n");
    exit(1);
  }
  if (ov->outp.pfp_pmc_count != 1) {
    printf("dispatch pmc count fails\n");
    exit(1);
  }
  if (ov->outp.pfp_pmd_count != 1) {
    printf("dispatch pmd count fails\n");
    exit(1);
  }
  if (ov->outp_mod.ibsop_base != 0) {
    printf("dispatch mod base fails\n");
    exit(1);
  }
 
  ov->pc[0].reg_num=ov->outp.pfp_pmcs[0].reg_num;
  ov->pc[0].reg_value=ov->outp.pfp_pmcs[0].reg_value;
 
  ov->pd[0].reg_num=ov->outp.pfp_pmds[0].reg_num;//pd[0].reg_num=7
  ov->pd[0].reg_value=0;
 
  ov->pd[0].reg_flags |= PFM_REGFL_OVFL_NOTIFY;
 
  ov->pd[0].reg_smpl_pmds[0]=((1UL<<7)-1)<<ov->outp.pfp_pmds[0].reg_num;
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 7);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 8);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 9);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 10);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 11);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 12);
  pfm_bv_set(ov->pd[0].reg_smpl_pmds, 13);

  smpl_arg_t buf_arg;
  void *buf_addr;
  entry_size = sizeof(smpl_entry_t)+(7<<3);
  ov->ctx.ctx_flags = 0;
  buf_arg.buf_size = getpagesize();

  int fd = pfm_create_context(&ov->ctx, PFM_DFL_SMPL_NAME, &buf_arg, sizeof(buf_arg));
  if(fd==-1){
    printf("cannot create context %s\n", strerror(errno));
  }
  ov->fd = fd;
  ov->tid =gettid();
  if(pfm_write_pmcs(ov->fd,ov->pc,ov->outp.pfp_pmc_count)){
	printf("write pmc error\n");
  }
 
  buf_addr =  mmap(NULL, (size_t)buf_arg.buf_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (buf_addr == MAP_FAILED)
    printf("cannot mmap sampling buffer\n");

  ov->hdr = (smpl_hdr_t *)buf_addr;

  if (PFM_VERSION_MAJOR(ov->hdr->hdr_version) < 1)
    printf("invalid buffer format version\n");


  if(pfm_write_pmds(ov->fd,ov->pd,ov->outp.pfp_pmd_count)){
	printf("write pmd error\n");
  }
 
  ov->load_args.load_pid=gettid();
 
  if(pfm_load_context(ov->fd, &ov->load_args)){
  }
 
  ret = fcntl(ov->fd, F_SETFL, fcntl(ov->fd, F_GETFL, 0) | O_ASYNC);
  if (ret == -1){
    printf("fcntl F_SETFL return -1\n");
  }

  #ifndef F_SETOWN_EX
  #define F_SETOWN_EX     15
  #define F_GETOWN_EX     16

  #define F_OWNER_TID     0
  #define F_OWNER_PID     1
  #define F_OWNER_GID     2

  struct f_owner_ex {
        int     type;
        pid_t   pid;
  };
  #endif
  {
    struct f_owner_ex fown_ex;
    fown_ex.type = F_OWNER_TID;
    fown_ex.pid  = gettid();
    ret = fcntl(ov->fd, F_SETOWN_EX, &fown_ex);
    if (ret == -1){
      printf("fcntl F_SETOWN return -1\n");
   }

   ret = fcntl(ov->fd, F_SETSIG, SIGIO);
     if(ret < 0){
       printf("cannot set SIGIO\n");
     }
   } 
    
  if(pfm_self_start(ov->fd)==-1){
	printf("start error with fd %d, %s\n", ov->fd, strerror(errno));
  } 
}
 
void stopIBS(int nthreads)
{
  printf("Begin to stop IBS\n");
  int i;
  for (i=0; i<nthreads; i++) {
	if(pfm_self_stop(fd2ov[i].fd)==-1){
		printf("stop error for %d\n", fd2ov[i].fd);
  	}
  }
}

void sigio_handler(int sig, siginfo_t* siginfo, void* context)
{
        ibsopdata3_t *opdata3;
        int count;
      	size_t pos;
	smpl_entry_t *ent;
	uint64_t *reg;
        int numa_access_node;
        int numa_local_node;

	int fd = siginfo->si_fd;
	pfarg_msg_t msg;
	int i;
	int threadId = -1;
	int cpu;
        int lat = 0;
        for (i=0; i<MAX_FD ; i++) {
	  if (fd2ov[i].fd == fd) {
	    threadId = i;
	    break;
          }
   	}
	struct over_args *ov = &fd2ov[threadId];
	if(ov->tid!=gettid())
		printf("a mismatch occur\n");
	else
	{
		ov->count++;
	}
        if (read(fd, &msg, sizeof(msg)) != sizeof(msg))
                printf("read from sigio fd failed\n");

        if(mallocFlag) {
	  if(pfm_restart(fd)){
	    printf("restart error\n");
            exit(1);
  	  }
          return;
	}

        // read entries in the buffer
	ent = (smpl_entry_t *)(ov->hdr+1);
	pos = (unsigned long)ent;
	count = ov->hdr->hdr_count;
	while(count--) {
	  cpu = ent->cpu;
	  int j; int n=7;
          reg = (uint64_t *)(ent+1);
          for (j = 0; n; j++) {
            if (pfm_bv_isset(ov->pd[0].reg_smpl_pmds, j)) {
              switch(j) {
                case 7:
                  if ((*reg & (1ull<<18)) == 0) {
                    //printf("no data captured\n");
                    goto skip;
                  }
                  break;
                case 9:
                  //opdata = (ibsopdata_t *)reg;
                  //break;
                case 10:
                  //opdata2 = (ibsopdata2_t *)reg;
                  break;
                case 11:
                  opdata3 = (ibsopdata3_t *)reg;
                  break;
                case 12:
                  if(((opdata3->reg.ibsldop == 1) || (opdata3->reg.ibsstop == 1)) && (opdata3->reg.ibsdclinaddrvalid == 1)) {
                    uint64_t addr = *reg;
                    //printf("addr is 0x%"PRIx64"\n", addr);
                    if(addr & 0xffff000000000000){

                       if(pfm_restart(fd)){
                          printf("restart error!\n");
                          exit(1);
                       }
                       return;
                    }
	            if((opdata3->reg.ibsldop == 1) && (opdata3->reg.ibsdcmiss == 1)){
	              lat = opdata3->reg.ibsdcmisslat;
	            }
                    //totalSample++;
                    uint64_t pagesize = getpagesize();
                    addr = addr & ~((pagesize - 1));

                    numa_access_node = getNumaNode(cpu);
                    numa_local_node = getLocalNode(addr);
                  }
                  break;
              }
              reg++; n--;
           }
         }

skip:
                pos += entry_size;
                ent = (smpl_entry_t *)pos;

  	}

	if(pfm_restart(fd)){
		printf("restart error\n");
          exit(1);
        }
		
}

void initIBS()
{
	struct over_args ov;
	struct sigaction sa;
	sigset_t set;

	printf("Begin to initialize IBS\n");
	memset(&ov, 0, sizeof(ov));

	memset(&sa, 0, sizeof(sa));
        sigemptyset(&set);
        sa.sa_sigaction = sigio_handler;
        sa.sa_mask = set;
        sa.sa_flags = SA_SIGINFO;
 
        if (sigaction(SIGIO, &sa, NULL) != 0)
                printf("sigaction failed");
 
        if (pfm_initialize() != PFMLIB_SUCCESS)
                printf ("pfm_initialize failed");
}
#endif

#if PEBS
#if PEBS_POW
/* THis is the handler called by PAPI_overflow*/
void handler(int EventSet, void *address, void *data_addr, unsigned long cpu,
                   unsigned long tid, unsigned long time, long long overflow_vector, void *context){
  
    //union perf_mem_data_src src = (perf_mem_data_src)data_src;

    if(mallocFlag)
      return;
    uint64_t dataAddr = (uint64_t)data_addr;
    if(dataAddr & 0xffff000000000000)
      return;
    cpu >>= 32;
       
    int pagesize = getpagesize(); 
    void *addr = (void *) (((long)data_addr) & ~((long)(pagesize - 1)));
    dataAddr = (uint64_t)addr;

    int i;

}

#else

/* THis is the handler called by PAPI_overflow*/
void handler(int EventSet, void *address, void *data_addr, unsigned long cpu, unsigned long tid, unsigned long time,
                   unsigned long weight, unsigned long src, long long overflow_vector, void *context){


    //int events[2];
    int retval;
    long long values[4];

    if ((retval = PAPI_read(EventSet, values)) != PAPI_OK)
       ERROR_RETURN(retval);

    {  
       if(mallocFlag)
          return;
       uint64_t dataAddr = (uint64_t)data_addr;
       if(dataAddr & 0xffff000000000000)
          return;

       union perf_mem_data_src data_src = (perf_mem_data_src)src;
       int pagesize = getpagesize();
       void *addr = (void *) (((long)data_addr) & ~((long)(pagesize - 1)));
       dataAddr = (uint64_t)addr; 

       int numa_access_node = getNumaNode(cpu);
       int numa_location_node = getLocalNode(dataAddr);

       // memery operation //
       char memOP[10] = "NA";
       if(data_src.mem_op & PERF_MEM_OP_LOAD)
          strncpy(memOP, "LOAD", 10);
       else if(data_src.mem_op & PERF_MEM_OP_STORE)
          strncpy(memOP, "STORE", 10);
       else if(data_src.mem_op & PERF_MEM_OP_PFETCH)
          strncpy(memOP, "PFETCH", 10);
       else if(data_src.mem_op & PERF_MEM_OP_EXEC)
          strncpy(memOP, "EXEC", 10); 

       // memory hierarchy info //
       char memHR[10] = "NON";
       if(data_src.mem_lvl & PERF_MEM_LVL_REM_RAM1 || data_src.mem_lvl & PERF_MEM_LVL_REM_RAM2)
          strncpy(memHR, "R_DRAM", 10);
       else if(data_src.mem_lvl & PERF_MEM_LVL_REM_CCE1 || data_src.mem_lvl & PERF_MEM_LVL_REM_CCE2)
          strncpy(memHR, "R_CAC", 10);
       else if(data_src.mem_lvl & PERF_MEM_LVL_LOC_RAM)
          strncpy(memHR, "L_DRAM", 10);       
       else if(data_src.mem_lvl & PERF_MEM_LVL_L1){
          	L1Counter++;
		return;// We didn't record the L1 memory access.
	}       
       else if(data_src.mem_lvl & PERF_MEM_LVL_L2)
          strncpy(memHR, "L_L2", 10);
       else if(data_src.mem_lvl & PERF_MEM_LVL_L3)
          strncpy(memHR, "L_L3", 10);       
       else if(data_src.mem_lvl & PERF_MEM_LVL_LFB)
          strncpy(memHR, "L_LFB",10);
       else if(data_src.mem_lvl & PERF_MEM_LVL_IO)
          strncpy(memHR, "L_IO",10);

       uint64_t memSIZE;
       int mallocNum;
       int memThreadID;
       int memID;
       if(PROFILE_FLAG){
          struct mallocInformation* mallocInfo= setMallocInter(dataAddr, weight);
          memSIZE = mallocInfo->size;
	  memID = mallocInfo->MallocID;
	  memThreadID = mallocInfo->threadID;
       }

       mallocNum = getMallocNum(0);
       
       struct Records *ptr = (struct Records *)real_malloc(sizeof(struct Records));
       if(ptr == NULL)
          ERROR_RETURN(1);
       sprintf(ptr->str,"%llu,%lu,%d,%d,%d,%p,%p,%s,%lu,%s,%lu,%d,%d,%d\n", values[1], cpu, gettid(), numa_access_node, numa_location_node, address, data_addr, memOP, memSIZE, memHR, weight, memID, memThreadID, mallocNum);
       ptr->next = NULL;

       curr->next = ptr;
       curr = ptr;
       
       SamNumber++;

    }
    if(SamNumber == 100){

       struct Records *ptr = (struct Records *)real_malloc(sizeof(struct Records));
       if(ptr==NULL)
          ERROR_RETURN(1);
        
       sprintf(ptr->str,"L1Counter,%lu,%lu\n",cpu,L1Counter);
       ptr->next = NULL;

       curr->next = ptr;
       curr = ptr;


       SamNumber = 0;
    }
}
#endif


/////*************************************************/////

void monitorInit()
{
	/* must be set to null before calling PAPI_create_eventset */
   int retval;
   
   char event_name[PAPI_MAX_STR_LEN];

   long long values[4];

   /* Here we create the eventset */
   if ((retval=PAPI_create_eventset (&EventSet)) == PAPI_EINVAL) {
      printf("error is EventSet is %d, %s\n", EventSet, strerror(errno));
      ERROR_RETURN(retval);
   }
   retval = PAPI_event_name_to_code("MEM_TRANS_RETIRED:LATENCY_ABOVE_THRESHOLD", &PAPI_event);
   if ( retval != PAPI_OK ) {
      printf( "MEM_TRANS_RETIRED not found\n");
   }

   int LLC_MISS_REMOTE;
   retval = PAPI_event_name_to_code("MEM_LOAD_UOPS_LLC_MISS_RETIRED:REMOTE_DRAM", &LLC_MISS_REMOTE);
   if ( retval != PAPI_OK) {
      printf("LLC_MISS_REMOTE not found\n");
   }

/*
   int LLC_MISS;
   retval = PAPI_event_name_to_code("LLC_MISSES", &LLC_MISS);
   if ( retval != PAPI_OK) {
      printf("LLC_MISSES not found\n");
   }
*/
   /* Here we are querying for the existence of the PAPI presets  */
   if (PAPI_query_event (PAPI_event) != PAPI_OK)
   {
      PAPI_event = PAPI_TOT_CYC;

      if ((retval=PAPI_query_event (PAPI_event)) != PAPI_OK)
         ERROR_RETURN(retval);

      printf ("PAPI_TOT_INS not available on this platform.");
      printf (" so subst PAPI_event with PAPI_TOT_CYC !\n\n");
   }






   /* PAPI_event_code_to_name is used to convert a PAPI preset from 
     its integer value to its string name. */
   if ((retval = PAPI_event_code_to_name (PAPI_event, event_name)) != PAPI_OK)
      ERROR_RETURN(retval);

   /* add event to the event set */

   if ((retval = PAPI_add_event (EventSet, PAPI_event)) != PAPI_OK){
        printf("It is wrong in thread %d\n",threadid);
	ERROR_RETURN(retval);
	}
   if ((retval = PAPI_add_event (EventSet, PAPI_TOT_CYC)) != PAPI_OK){
        printf("It is wrong in thread %d\n",threadid);
	ERROR_RETURN(retval);
	}
/*
   //printf("LLC_MISS is: %d\n",LLC_MISS);
   if ((retval = PAPI_add_event(EventSet, LLC_MISS_REMOTE)) != PAPI_OK){
      printf("It is wrong in thread %d\n",threadid);
      ERROR_RETURN(retval);
	}
*/
   /* register overflow and set up threshold */
   /* The threshold "THRESHOLD" was set to 100000 */
   if ((retval = PAPI_overflow (EventSet, PAPI_event, THRESHOLD, 0,
		                       handler)) != PAPI_OK)
      ERROR_RETURN(retval);

   /* Start counting */
   if ( (retval=PAPI_start (EventSet)) != PAPI_OK)
      ERROR_RETURN(retval);

//   unsigned long long timeStamp = rdtsc();

   if ( (retval=PAPI_read(EventSet, values)) != PAPI_OK)
      ERROR_RETURN(retval);

   struct Records *ptr = (struct Records *)real_malloc(sizeof(struct Records));
   if(ptr==NULL)
      ERROR_RETURN(1);
  
   sprintf(ptr->str,"%lld, %lld\n",values[1], values[2]);
 
   ptr->next = NULL;

   curr->next = ptr;
   curr = ptr;

   L3DCM = values[2];
}
void monitorEnd()
{
   long long values[3];
   int retval;

/*
   printf("Thread %d prepares to end *************************finish0\n",threadid);
   struct Records *ptr = (struct Records *)real_malloc(sizeof(struct Records));
   printf("Thread %d prepares to end *************************finish1\n",threadid);
   if(ptr==NULL)
       ERROR_RETURN(1);
   printf("Thread %d prepares to end *************************finish2\n",threadid); 
 */ 
  /* Stops the counters and reads the counter values into the values array */
    printf("Thread %d prepares to end *************************finish0\n",threadid);
    if ( (retval=PAPI_stop (EventSet, values)) != PAPI_OK){
	printf("It is wrong in thread %d\n",threadid);
	ERROR_RETURN(retval);
    }
    printf("Thread %d prepares to end *************************finish1\n",threadid);

    struct Records *ptr = (struct Records *)real_malloc(sizeof(struct Records));
    if(ptr==NULL)
       ERROR_RETURN(1);
    printf("Thread %d prepares to end *************************finish2\n",threadid);
    sprintf(ptr->str,"%lld, %lld\n",values[1],values[2]);
 
    ptr->next = NULL;

    curr->next = ptr;
    curr = ptr;

   /* clear the overflow status */
   if ((retval = PAPI_overflow (EventSet, PAPI_event, 0, 0,
		                       NULL)) != PAPI_OK)
      ERROR_RETURN(retval);
    printf("Thread %d prepares to end *************************finish3\n",threadid);
    /************************************************************************
    * PAPI_cleanup_eventset can only be used after the counter has been    *
    * stopped then it remove all events in the eventset                    *
    ************************************************************************/
   if ( (retval=PAPI_cleanup_eventset (EventSet)) != PAPI_OK)
      ERROR_RETURN(retval);
   /* Free all memory and data structures, EventSet must be empty. */
    printf("Thread %d prepares to end *************************finish4\n",threadid);
   if ( (retval=PAPI_destroy_eventset(&EventSet)) != PAPI_OK)
      ERROR_RETURN(retval);
   printf("Thread %d prepares to end *************************finish5\n",threadid);
  /* free the resources used by PAPI */ 
  //if(threadid==0)
  //	PAPI_shutdown();
}

void initPapi()
{
   int retval;
   isPapiInitialized = 0;
   /****************************************************************************
   *  This part initializes the library and compares the version number of the *
   * header file, to the version of the library, if these don't match then it  *
   * is likely that PAPI won't work correctly.If there is an error, retval     *
   * keeps track of the version number.                                        *
   ****************************************************************************/
   if ((retval = PAPI_library_init (PAPI_VER_CURRENT)) != PAPI_VER_CURRENT)
   {
      printf("Library initialization error! %d \n",retval);
      exit(1);
   }

   
   isPapiInitialized = 1;
}

#endif

initCPUmap(){
    int i;
    Nodes = 2;
    for(i=0; i< MAX_CPU; ++i)
       cpu_node_map[i] = -1;
}

void recordingSamples(int tid){

  char file[50];
  sprintf(file,"cycSample%d-%d-%d.txt",THRESHOLD,THRESHOLD_CYC,tid);

  printf("writing samples to file %s\n",file);
  struct Records *ptr;
  ptr = head;

  FILE *sample;
  sample = fopen(file,"w");

  while(ptr != NULL){
     fprintf(sample,"%s",ptr->str);
     ptr = ptr->next;
  }
  fclose(sample);

}

void recordingStacks(int tid){

  char file[50];
  sprintf(file,"cycStack%d-%d-%d.txt",THRESHOLD,THRESHOLD_CYC,tid);

  printf("writing stacks to file %s\n",file);
  struct stack_Records *ptr;
  ptr = stack_head;

  FILE *sample;
  sample = fopen(file,"w");

  while(ptr != NULL){
     fprintf(sample,"%s",ptr->str);
     ptr = ptr->next;
  }
  fclose(sample);

}



// Initialization for the whole program, create the linkedlist for recording sample
void * monitor_init_process(int *argc, char **argv, void *data)
{
#if PEBS

    initPapi();	 
    backtraceFlag=1; 
    create_list();
    
    NUM_CPU = sysconf(_SC_NPROCESSORS_ONLN);
    monitorInit();
#elif IBS
        NUM_CPU = sysconf(_SC_NPROCESSORS_ONLN);
        initIBS();
        startIBS(0);
#endif
}
void monitor_fini_process(int how, void *data)
{
#if PEBS
      
       monitorEnd();
       PAPI_shutdown();
       recordingSamples(threadid);
       recordingStacks(threadid);
#endif
}

// Initialization for each thread, create the linkedlist for recording sample
void* monitor_init_thread(int tid, void* data)
{
	int retval;
	create_list();
        backtraceFlag=1;
	retval = PAPI_thread_init( ( unsigned long ( * )( void ) )( pthread_self ) );
   	if ( retval != PAPI_OK ) {
         	ERROR_RETURN( retval );
   	}
	retval = PAPI_register_thread( );
 	if ( retval != PAPI_OK ) {
	 	ERROR_RETURN( retval );
 	}
        threadid = tid;
        printf("Thread %d begins\n",tid);
	monitorInit();
}

// Do the jobs after sampleing and record samples to file.
void monitor_fini_thread(void* init_thread_data)
{
	int retval;
	monitorEnd();
	retval = PAPI_unregister_thread( );
 	if ( retval != PAPI_OK )
	   ERROR_RETURN( retval );
	printf("Thread %d ends\n",threadid);
        recordingSamples(threadid);
	recordingStacks(threadid);
}


int main()
{
   
  return 0;
}
