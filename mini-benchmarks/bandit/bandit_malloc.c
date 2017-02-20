#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

#include "help.h"

// allocate 640M Memory
#define LENGTH (640UL*1024*1024)
#define PROTECTION (PROT_READ | PROT_WRITE)

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif

#define ADDR (void *)(0x0UL)
#define FLAGS (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)

#define LINE_SIZE (32)

// MAGIC should be large enough to avoid pretching effects on different streams
//#define MAGIC 1
long num = 10000000;
int
main(int argc, char **argv)
{
  int i;

  if (argc != 4) {
    printf("the usage is: bandit #stream #stride #magic, stream is between 1 to %d\n", 
	   512*1024/64/7);
    exit(0);
  }

  int stream = atoi(argv[1]);
  if (stream <= 0) {
    printf("please specify a positive stream number\n");
    exit(0);
  }

  int stride = atoi(argv[2]);
  if (stride <= 0) {
    printf("please specify a positive stride number\n");
    exit(0);
  }

  int magic  = atoi(argv[3]);
  if (magic <= 0) {
    printf("please specify a positive magic number\n");
    exit(0);
  }

  uint64_t *cur = (uint64_t *)malloc(stream * sizeof(uint64_t));
  char *tmp = (char *)malloc(stream * sizeof(char));
  if (!cur || !tmp) {
    printf("cannot allocate valid memory\n");
    exit(0);
  }

  memset(tmp, 0, stream * sizeof(char));

  for (i = 0; i < stream; i++) {
    cur[i] = magic*i*LINE_SIZE; // give MAGIC to make sure the prefetching does not work
  }
  

    
  void *addr;
  addr=(char *)malloc(LENGTH * sizeof(char)); 


  //addr = mmap(ADDR, LENGTH, PROTECTION, FLAGS, 0, 0);
  if (addr == MAP_FAILED) {
    perror("mmap allocation");
    exit(1);
  }
  
  //char *addr = (char *)malloc( LENGTH * sizeof(char));
  memset(addr, 0, LENGTH);
  volatile char *array = (char *)addr;

  while(num>0) {
    // number of stream can be configured
    for(i = 0; i < stream; i++) {
      if (cur[i] >= LENGTH) cur[i] = magic*i*LINE_SIZE;
      tmp[i] = array[identity(tmp[i])+cur[i]];
      int n = 1;
      cur[i] += stride * n;
    }
    num --;
  }

  free(addr);
  return 0;
}
