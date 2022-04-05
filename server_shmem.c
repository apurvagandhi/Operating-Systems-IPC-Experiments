// server_shmem.c
// student name: Apurva Gandhi
// Simple POSIX shared memory server.
// author: kwalsh@holycross.edu
// date: 4 March 2022
// updated: 23 February 2020, converted from C++ to C
// updated: 23 February 2020, changed from banking to event-logging
// updated: 15 February 2022, rewrote code for new project

// This implements the server half of a toy event-logging system. The server
// keeps statistics about the frequency and details of various "event"
// occurrences. Each type of event will have an ID number (EID, a number from 0
// to 1023), a short one-word name (up to 15 characters), and a longer
// description (up to 63 characters).
// 
// The server maintains a table, indexed by event type ID, containing:
//   - the name for that event type
//   - the description for that event type
//   - a count of how many occurences have been reported for that event type
// Initially the server's table will be empty. The server then waits for IPC
// messages from clients asking to do things like:
//  - register a new event type, i.e. setting the name and description
//  - report an event occurrence, incrementing one of the event counters
//  - reset an event type, zeroing one of the event counters
//  - print out a summary of all event statistics

/******************************************************************
* The calculation can be verified using <region_name> experiment <count> command 
* Client's count	Throughput  Throughput	Throughput	Throughput (Average)
               10	19582.92291	19838.71128	19374.10032	19598.57817
              100	16275.36618	15240.02506	12445.12478	14100.1817
             1000   12211.43539	12375.18834	12430.36556 12338.99643
            10000   13466.23299	14346.5665	14850.39806 14221.06585
          1000000   14601.48485 15040.47149 14817.36223 14819.77286
        1 Million	14917.82306	15039.84665	            14978.83485 

For my experiments, all of my results have been fairly consistent
and almost similar regardless of the count used by client. Throughput 
which is roundtrips/second is high for count 10 but mostly close to 15000
as the client's count increases. I would say count of 1,000,000 is more
useful because using small count might just happen to create noise.
Because count of 10,100, or 1000 is small, it might take a little extra
time for CPU to wake up and start its operation. Therefore, using large number
of count is probably best to estimate. In this case, I would say 1,000,000
is the best to balance not too big but not too small of count.   

*Average round trip time for count 1,000,000 is 0.000067

*The maximum count that is valid is 2 billion because server uses 
int as the type to store the count variable. There is a limit
on value for integer type to ~2 billion and therefore, 
maximum count that is valid is about 2 billion. 

*I am using radius for this experiment. 

*Statistics:
Elapsed time: 67.077656 seconds
Elapsed time: 67077.656014 milliseconds
Elapsed time: 67077656.014000 microseconds
Elapsed time: 67077656014.000000 nanoseconds
Total number of round trips completed are 1000000.
Throughput is 14908.093983 round-trips/second
Average round trip time is 0.000067
******************************************************************/




#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>

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

// Global variables

// This struct holds information and statistics for one event type.
struct event_stats {
    char *name;
    char *description;
    int count;
};

// Global variables
struct event_stats stats[1024]; // table of info about all possible events
char *name = NULL; // name of the shared memory region

// Print stats about all events
void print_stats() {
    printf("%4s %15s %63s %10s\n", "ID", "Name", "Description", "Count");
    for (int i = 0; i < 1024; i++) {
        if (stats[i].name == NULL)
            continue;
        printf("%4d %15s %63s %10d\n", i, stats[i].name, stats[i].description, stats[i].count);
    }
}

// This function gets invoked whenever the user presses Control-C.
void cleanup(int s) {

    // Remove the shared memory region.
    if (name != NULL)
        shm_unlink(name);

    // Print a friendly message then exit.
    printf("Final event statistics...\n");
    print_stats();
    exit(1); 
}

// Register a new event type by setting the name and description. The data array
// should contain a name and a description, separated by a space and terminated
// with a NUL character.
void register_event_type(int eventid, char *data) {
    if (eventid < 0 || eventid >= 1024) {
        printf("ERROR: can't register event ID %d\n", eventid);
        return;
    }
    char *p = strchr(data, ' ');
    if (p == NULL) {
        printf("ERROR: can't register event ID %d, data is missing space separator\n", eventid);
        return;
    }
    *p = '\0';
    p++;
    stats[eventid].name = strdup(data);
    stats[eventid].description = strdup(p);
    printf("Registered new event type: ID=%d name='%s' description='%s'\n",
            eventid, stats[eventid].name, stats[eventid].description);
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
    if (fd < 0) {
        perror("shm_open");
        printf("Can't create shared memory region.\n");
        return -1;
    }
    printf("Created shared memory region \"%s\".\n", name);

    // "Truncate" the region so it is exactly the size we want
    int err = ftruncate(fd, sizeof(struct shared_stuff));
    if (err != 0) {
        perror("ftruncate");
        printf("Can't resize shared memory region.\n");
        return -1;
    }

    // Get a pointer to the start of the region.
    void *ptr = mmap(0, sizeof(struct shared_stuff), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        printf("Can't map shared memory region.\n");
        return -1;
    }
    struct shared_stuff * volatile p = (struct shared_stuff *)ptr;

    while (1) {
        while (p->operation == 0) {
            // do nothing
        }
        //printf("Shared memory has changed: operation=%d eventid=%d\n", p->operation, p->eventid);
        if (p->operation == 1) {
            register_event_type(p->eventid, p->data); // register event type
        } else if (p->operation == 2) {
            stats[p->eventid].count++; // report event occurrence
        } else if (p->operation == 3) {
            print_stats(); // also print statistics, for debugging purposes.
            stats[p->eventid].count = 0; // reset event counter
        } else {
            printf("Sorry, I don't know what to do for operation %d.\n", p->operation);
        }
        p->operation = 0; // reset the operation to be ready for the next transaction
    }

    printf("All done!\n");
    cleanup(0);
    return 0;
}
