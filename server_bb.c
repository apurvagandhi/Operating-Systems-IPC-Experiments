// server_bb.c
// student name: Apurva Gandhi
// Measuring Shared-Memory Bulk Transfer Throughput with Bounded Buffers
// author: ahgand22@holycross.edu
// Date: 4 March 2022

/******************************************************************
* Long duration of the sleep: 1.32 MB/sec, 1.44 MB/sec, 1.46 MB/sec
* Short (2) duration of the sleep: 1.70 MB/sec, 1.74 MB/sec, 1.66 MB/sec
* 0 length sleep: 1.84 MB/sec, 1.76 MB/sec, 1.78 MB/sec, 1.84 MB/sec
* Abscence of sleep: 24 MB/sec, 29 MB/sec, 29 MB/sec, 27 MB/sec, 22 MB/sec
* The presence of the sleep does affect the performance of the program. 
* Something that can explain this is because when usleep() is present, 
* system call is being made. systemcall is expensive and therefore, 
* absence of usleep() makes the performance better. Presence and the 
* length does not affect much because it's the microsecond
* and therefore, if it's low number only fraction of second is saved 
* and even if it's big number, not much is different since it has to 
* make system call and that is the biggest cost. 

* Count: 1000000
* Buffer size: 2 - throughput: N/A because array is always full since one needs to 
* be empty and therefore, it is always full. 
* Buffer size: 16 - throughput: 64.75 MB/second, 79.49 MB/second, 22.50 MB/second, 19.981MB/second, 23.92MB/second
* Buffer size: 256 - throughput: 49.10 MB/sec, 37.91 MB/sec, 46.81 MB/sec
* Buffer size: 997 - throughput: 36.938877 MB/second, 28.052931 MB/second, 37.291447 MB/second, 29.163066 MB/second
* Buffer size: 1024 - throughput: 80.563292 MB/second, 116.891493 MB/second, 77.328498 MB/second
* Buffer size: 1024 X 1024 - throughput: 105.691922 MB/second, 81.372851 MB/second, 89.256602 MB/second
* I do not think the size of buffer have much effect on the throughput. 
* I do notice one weird change once in a while where some throughput are above 80 MB/sec 
* while some are in 30s and 40s. However, I do not think that is because of the change 
* of buffer size. I do see high throughput with the increase in buffer size, but
* sometimes there is high throughput even with small buffer size. 

* The best data throughput my  server can achieve: 111.688310 MB/second
* with array size of 1024X1024 and count of 1000000.

*I am using radius for this experiment. 

*Statistics:
Opened shared memory region "agandhi-shared-region".
Elapsed time: 0.035814 seconds
Elapsed time: 35.813954 milliseconds
Elapsed time: 35813.954000 microseconds
Elapsed time: 35813954.000000 nanoseconds
Total sum is: 38038049.
Total number of round completed are 1000000.
Throughput is 111.688310 MB/second

******************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>

struct shared_stuff 
{
    int in;
    int out;
    int buffer [1024*1024];
    int totalValue;
};

// Global variables
char *name = NULL; // name of the shared memory region

// This function gets invoked whenever the user presses Control-C.
void cleanup(int s) {

    // Remove the shared memory region.
    
    if (name != NULL)
        shm_unlink(name);
    exit(1); 
}

int main(int argc, char **argv)
{
    // This next code registers a signal handler, so that if the user presses
    // Control-C, then we still have the chance to cleanup (i.e. delete the IPC
    // mailbox queue). After all, we don't want to pollute radius with lots of
    // defunct mailbox queues.
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = cleanup;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    if (argc != 2) {
        printf("usage: %s <region_name>\n", argv[0]);
        printf("  You can use any name you like for the region, but\n");
        printf("  by convention the name is usually of the form: \"/something\"\n");
        printf("  and it must be unique to you (if another person has already\n");
        printf("  created that region, you won't be able to).\n");
        exit(1);
    }
    name = argv[1];

    int fd = shm_open(name, O_CREAT | O_RDWR, 0660);
    if (fd < 0) 
    {
        perror("shm_open");
        printf("Can't create shared memory region.\n");
        return -1;
    }
    printf("Created shared memory region \"%s\".\n", name);

    // "Truncate" the region so it is exactly the size we want
    int err = ftruncate(fd, sizeof(struct shared_stuff));
    if (err != 0) 
    {
        perror("ftruncate");
        printf("Can't resize shared memory region.\n");
        return -1;
    }

    // Get a pointer to the start of the region.
    void *ptr = mmap(0, sizeof(struct shared_stuff), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) 
    {
        perror("mmap");
        printf("Can't map shared memory region.\n");
        return -1;
    }

    struct shared_stuff * volatile p = (struct shared_stuff *)ptr;
    size_t size = sizeof(p->buffer) / sizeof(int);
    
    while (1) 
    {
        //wait until buffer is NOT empty
        while(p->in == p->out)
        {
            //do nothing;
            //usleep(15);
        }
        p->totalValue += p->buffer[p->out];
        p->buffer[p->out] = 0;
        p->out = ((p->out+1) % (size-1));
    }

    cleanup(0);
    return 0;
}

