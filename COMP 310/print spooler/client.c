#include "common.h"

int fd;
Shared* shared_mem;

int setup_shared_memory() {
    fd = shm_open(MY_SHM, O_RDWR, 0666);
    if (fd == -1) {
        printf("shm_open() failed\n");
        exit(1);
    }
}

int attach_shared_memory() {
    shared_mem = (Shared*) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_mem == MAP_FAILED) {
        printf("mmap() failed\n");
        exit(1);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    setup_shared_memory();
    attach_shared_memory();

    if (argc < 3) {
        printf("You must input a client ID and a number of pages\n");
        exit(1);
    }

    // produce item
    job clientJob;
    clientJob.id = atoi(argv[1]);
    clientJob.pages = atoi(argv[2]);

    int semvalue;
    sem_getvalue(&(shared_mem->empty), &semvalue);
    if (semvalue == 0) {
        printf("Client %d has %d pages to print, buffer full, sleeps\n", clientJob.id, clientJob.pages);
    }

    sem_wait(&(shared_mem->empty)); // empty--
    sem_wait(&(shared_mem->binary));

    // insert item
    int index = shared_mem->tailIndex;
    shared_mem->tailIndex = (shared_mem->tailIndex + 1) % shared_mem->bufferSize;

    job *bufferJob = &(shared_mem->buffer[index]);
    bufferJob->id = clientJob.id;
    bufferJob->pages = clientJob.pages;

    if (semvalue == 0)
        printf("Client %d wakes up, puts request in Buffer[%d]\n", bufferJob->id, index);
    else
        printf("Client %d has %d pages to print, puts request in Buffer[%d]\n", bufferJob->id, bufferJob->pages, index);

    sem_post(&(shared_mem->binary));
    sem_post(&(shared_mem->full));

    // release shared memory
    munmap(shared_mem, sizeof(Shared));

    return 0;
}
