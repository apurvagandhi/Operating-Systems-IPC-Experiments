// client_shmem.c
//Student Name: Apruva Gandhi
// Simple POSIX shared memory client.
// author: kwalsh@holycross.edu
// date: 3 January 2016
// updated: 23 February 2020, converted from C++ to C
// updated: 23 February 2020, changed from banking to event-logging
// updated: 15 February 2022, rewrote code for new project

// This implements the client half of a toy event-logging system. Currently, it
// just sends a single IPC message to the server, depending on command line
// arguments.

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

// This struct will contain all the shared data. There is no required format,
// and we can put anything we like into it. The idea is that a client can put
// info into the operation, eventid, and data fields. The server will then
// perform the action and reset the other fields back to zero.
//
// Shared memory does not have any built-in synchronization: the server and
// client are both able to access all the fields, even while the other is busy
// modifying the fields. The correct way to synchronize involves locks, mutexes,
// semaphores, etc., to ensure that, for example, the server doesn't start a
// deposit operation until _after_ the client has finished setting the
// operation, eventid, and data fields.
//
// Since we don't know how to use locks, mutexes, or semaphores, we will use a
// very simple synchronization protocol, as follows:
//
// * The client will not do anything until the operation is zero.
// * Once the operation is zero, the client will then set the eventid and data,
//   and finally the operation. The operation is the _last_ thing the client sets.
//
// * The server will not do anything until operation is non-zero.
// * Once the operation is non-zero, the server will perform the operation, and
//   set the operation back to zero. This indicates that the server is done with
//   the transaction. 
//
// NOTE: If you change this struct, you need to change it in client_shmem.c too.
struct shared_stuff {
    int operation;  // requested operation (1 = register, 2 = report, 3 = reset, etc.)
    int eventid;    // the event type ID
    char data[100]; // other data (up to 100 bytes)
};

int main(int argc, char **argv)
{

    if (argc < 4) 
    {
        printf("usage: %s <region_name> [ register <id> <name> <desc> | reset <id> | report <id> | experiment <count> ]\n", argv[0]);
        printf("  You can use any name you like for the region, but\n");
        printf("  by convention the name is usually of the form: \"/something\"\n");
        printf("  and it must be unique to you (if another person has already\n");
        printf("  created that region, you won't be able to).\n");
        exit(1);
    }

    char *name = argv[1];

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

    printf("Waiting until shared memory is not busy.\n");
    while (p->operation != 0)
        usleep(1);
    
    struct timespec t_end;
    struct timespec t_start;
    int numReports = 0;

    // Now that memory is not busy, do one operation, depending on command line parameters.
    if (!strcmp(argv[2], "register")) 
    {
        if (argc != 6) 
        {
            printf("you must provide event id, name, and description");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        char *name = argv[4];
        char *desc = argv[5];
        printf("Writing an operation in shared memory regiion to register new event type %d with name %s and description %s\n",
                eventid, name, desc);
        p->eventid = eventid;
        sprintf(p->data, "%s %s", name, desc);
        // note: operation needs to happen _last_
        p->operation = 1; // 1 means "register
    } 
    else if (!strcmp(argv[2], "report")) 
    {
        if (argc != 4) 
        {
            printf("you must provide event id");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        printf("Writing an operation in shared memory to report occurrence of event type %d\n", eventid);
        p->eventid = eventid;
        strcpy(p->data, ""); // data is not used here
        // note: operation needs to happen _last_
        p->operation = 2; // 1 means "report"
    } 
    else if (!strcmp(argv[2], "reset")) 
    {
        if (argc != 4) 
        {
            printf("you must provide event id");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        printf("Writing an operation in shared memory to reset statistics for event type %d\n", eventid);
        p->eventid = eventid;
        strcpy(p->data, ""); // data is not used here
        // note: operation needs to happen _last_
        p->operation = 3; // 3 means "reset"
    } 
    else if(!strcmp(argv[2], "experiment"))
    {
        if (argc != 4) 
        {
            printf("you must provide count");
            exit(1);
        }
        //register 
        int eventid = 1;
        char *name = "Installation";
        char *desc = "InstallationFailed";
        p->eventid = eventid;
        sprintf(p->data, "%s %s", name, desc);
        // note: operation needs to happen _last_
        p->operation = 1; // 1 means "register

        //Report and Reset
        int count = atoi(argv[3]);
        while(numReports != count+1)
        {
            //waiting for server to get finished
            while (p->operation != 0)
            {
                usleep(1);
            }
            
            int eventid = 1;
            p->eventid = eventid;
            //Reset
            //if it's the last transaction
            if(numReports == count)
            {
                //stop the timer
                clock_gettime(CLOCK_MONOTONIC, &t_end);
                numReports++;
                //change operation to reset
                p->operation = 3;
            }
            else
            {
                //if it's the first transaction, turn on the timer
                if(numReports == 0)
                {
                    clock_gettime(CLOCK_MONOTONIC, &t_start);
                }
                numReports++;
                p->operation = 2; //report
            }
        }
    }
    else 
    {
        printf("Sorry, I don't know how to do '%s'\n", argv[2]);
        exit(1);
    }

    int seconds = t_end.tv_sec - t_start.tv_sec;
    int nanoseconds = t_end.tv_nsec - t_start.tv_nsec;
    double t = seconds + nanoseconds / 1e9;

    // print elapsed time {in various units, just for variety)
    printf("Elapsed time: %0.6f seconds\n", t);
    printf("Elapsed time: %0.6f milliseconds\n", t * 1e3);
    printf("Elapsed time: %0.6f microseconds\n", t * 1e6);
    printf("Elapsed time: %0.6f nanoseconds\n", t * 1e9);
    printf("Total number of round trips completed are %i.\n", numReports-1);
    printf("Throughput is %f round-trips/second\n", (numReports-1)/t);
    printf("Average round trip time is %f seconds.\n", t/(numReports-1));
    printf("Waiting until server completes the transaction.\n");
    while (p->operation != 0)
        usleep(1);

    printf("All done!\n");
    return 0;
}

