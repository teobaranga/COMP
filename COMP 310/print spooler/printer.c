#include "common.h"
#include <unistd.h>
#include <signal.h>

/** File descriptor of the shared memory */
int fd;

int errno;

/** Size of buffer */
int MY_LEN;

/** Time it takes to print a page in seconds */
const float TIME_PER_PAGE = 0.25;

Shared* shared_mem;

void intHandler(int dummy) {
    munmap(shared_mem, sizeof(Shared) + MY_LEN * sizeof(job));
    shm_unlink(MY_SHM);
    exit(0);
}

int setup_shared_memory() {
    fd = shm_open(MY_SHM, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        printf("shm_open() failed\n");
        exit(1);
    }
    // Set size of shared memory
    ftruncate(fd, sizeof(Shared) + MY_LEN * sizeof(job));
}

int attach_shared_memory() {
    shared_mem = (Shared*)  mmap(NULL, sizeof(Shared) + MY_LEN * sizeof(job), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shared_mem == MAP_FAILED) {
        printf("mmap() failed\n");
        exit(1);
    }
    return 0;
}

int init_shared_memory() {
    shared_mem->headIndex = 0;
    shared_mem->tailIndex = 0;
    shared_mem->bufferSize = MY_LEN;
    sem_init(&(shared_mem->binary), 1, 1);
    sem_init(&(shared_mem->empty), 1, MY_LEN - 1);
    sem_init(&(shared_mem->full), 1, 0);
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("You must provide a number of free slots\n");
        exit(1);
    }

    signal(SIGINT, intHandler);

    printf("%d\n", atoi(argv[1]));
    MY_LEN = atoi(argv[1]);

    setup_shared_memory();
    attach_shared_memory();

    init_shared_memory();

    job printJob;

    while (1) {

        int semvalue;
        sem_getvalue(&(shared_mem->full), &semvalue);
        if (semvalue == 0) {
            printf("No requests in buffer, printer sleeps\n");
        }

        sem_wait(&(shared_mem->full));
        sem_wait(&(shared_mem->binary));

        // take a job
        int index = shared_mem->headIndex;
        shared_mem->headIndex = (shared_mem->headIndex + 1) % shared_mem->bufferSize;

        job *bufferJob = &(shared_mem->buffer[index]);
        printJob.id = bufferJob->id;
        printJob.pages = bufferJob->pages;

        sem_post(&(shared_mem->binary));
        sem_post(&(shared_mem->empty));

        // print job
        printf("Printer starts printing %d pages from client %d at Buffer[%d]\n", printJob.pages, printJob.id, index);

        // go to sleep
        usleep(printJob.pages * TIME_PER_PAGE * 1000000);

        printf("Printer finishes printing %d pages from client %d at Buffer[%d]\n", printJob.pages, printJob.id, index);
    }

    return 0;
}
