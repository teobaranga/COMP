#ifndef _INCLUDE_COMMON_H_
#define _INCLUDE_COMMON_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// from `man shm_open`
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <semaphore.h>

#define MY_SHM "/TEO"

typedef struct {
	int id;
	int pages;
} job;

typedef struct {
	sem_t binary;
	sem_t empty;
	sem_t full;
	int headIndex;
	int tailIndex;
	int bufferSize;
	job buffer[0];
} Shared;

#endif //_INCLUDE_COMMON_H_
