// client_bb.c
// student name: Apurva Gandhi
// Measuring Shared-Memory Bulk Transfer Throughput with Bounded Buffers
// author: ahgand22@holycross.edu
// Date: 4 March 2022

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct shared_stuff 
{
    int in;
    int out;
    int buffer [1024*1024];
    int totalValue;
};

int main(int argc, char **argv)
{
    if (argc < 3) 
    {
        printf("usage: %s <region_name> [count]\n", argv[0]);
        printf("  You can use any name you like for the region, but\n");
        printf("  by convention the name is usually of the form: \"/something\"\n");
        printf("  and it must be unique to you (if another person has already\n");
        printf("  created that region, you won't be able to).\n");
        exit(1);
    }
    char *name = argv[1];
    int count = atoi(argv[2]);
    struct timespec t_end;
    struct timespec t_start;

    int fd = shm_open(name, O_RDWR, 0);
    if (fd < 0) 
    {
        perror("shm_open");
        printf("Can't open shared memory region.\n");
        return -1;
    }
    printf("Opened shared memory region \"%s\".\n", name);

    // Get a pointer to the start of the region.
    void *ptr = mmap(0, sizeof(struct shared_stuff), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) 
    {
        perror("mmap");
        printf("Can't map shared memory region.\n");
        return -1;
    }

    struct shared_stuff * volatile p = (struct shared_stuff *)ptr;
    //getting size of a buffer
    size_t size = sizeof(p->buffer) / sizeof(int);
    int current = 0;
    while(current != count)
    {
        //starts the timer if it's the first round
        if(current == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &t_start);
        }
        //stops the timer if it's the last round
        else if(current == count-1)
        {
            //wait for server to finish reading the last received message
            while(p->in == p->out)
            {
                //usleep(15);
            }
            //stop the time
            clock_gettime(CLOCK_MONOTONIC, &t_end);
        }
        //Wait until buffer is not full
        while(p->out == ((p->in+1) % (size-1)))
        {
            //usleep(15);
        }
        //insert an item
        p->buffer[p->in] = 1;    
        current++;
        //update index of oldest unfilled position in buffer
        p->in = ((p->in + 1) % (size-1));
    }

    int seconds = t_end.tv_sec - t_start.tv_sec;
    int nanoseconds = t_end.tv_nsec - t_start.tv_nsec;
    double t = seconds + nanoseconds / 1e9;
    printf("Elapsed time: %0.6f seconds\n", t);
    printf("Elapsed time: %0.6f milliseconds\n", t * 1e3);
    printf("Elapsed time: %0.6f microseconds\n", t * 1e6);
    printf("Elapsed time: %0.6f nanoseconds\n", t * 1e9);
    printf("Total sum is: %i.\n", p->totalValue);
    printf("Total number of round completed are %i.\n", current);
    printf("Throughput is %f MB/second\n", ((current*4)/1000000.0)/t);
    
    return 0;
}

