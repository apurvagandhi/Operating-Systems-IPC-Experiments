// server_mpi.c
//Student Name: Apruva Gandhi
// Simple message-passing server using SystemV style IPC. 
// author: kwalsh@holycross.edu
// date: 3 January 2016
// updated: 23 February 2020, converted from C++ to C
// updated: 23 February 2020, changed from banking to event-logging
// updated: 15 February 2022, rewrote code for new project

// This implements the server half of a toy event-logging system. The server
// keeps statistics about the frequency and details of various "event"
// occurrences. Each type of event will have an ID number (EID, a number from 0
// to 1023), a short one-word name (up to 15 characters), and a longer
// description (up to 31 characters).
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

/******************************************************
******-------Experiement 3----------*************
*   count	Throughput  	Throughput	
     100	159433.437337	523590.363843		
    1000    433636.112522   552535.641312
   10000    413641.191581   383815.999224
  100000    519834.257917   605697.862788
 1000000    716801.004338   559806.007066
10000000    541233.201137   483774.095881

I think my event logging server is pretty fast here. 
By looking at the data above, it is roughly about
half a million report IPC messages per second. 

* It is faster than shared memory. For shared memory, the 
throughput is roughly 15,000 round-trips/second. Even if we consider
measuring one-way messages for shared memory, it would still be way slower than
SystemV IPC. One of the reason for this is because shared memory program
goes to sleep until the value has been changed. In other words, it has to 
wait for client or server to finish its "work". In SystemV, server does not 
wait for client to finish its work since it doesnot have to modify anything 
that server needs to use. Therefore, it simply does its work independent of the
client's work. Server simply waits for message to arrive and as soon as it 
receives it, it does the work. When it goes back upto the loop, it receives again
and it continues its work. No need to wait for client as in shared memory. SystemV
uses queue, msgsnd and msgrcv, uses queue and clients puts the data into 
the queue and server simply retrives the data. This is much more faster 
than waiting for shared memory client-server relatinoship where each 
has to wait for other to finish doing its work. That's why mpi is faster than
shared memory. 

* I am using radius for this experiment. 

* Statistics
    Registered new event type: ID=1 name='BatteryError' description='UnexpectedShutDown'
    ID            Name                     Description      Count
    1    BatteryError              UnexpectedShutDown    1000000
    Elapsed time: 2.436915 seconds
    number of report IPC messages received 1000000
    throughput is 410354.955379 report IPC messages per second

******-------Experiement 4----------*************
* The largest possible size kernel is willing to deliver is 8484 bytes. The default value
is 8192 bytes but the overhead cost is 8 bytes and so 8184 is the largest possible bytes
that can be sent.

Source: https://man7.org/linux/man-pages/man2/msgsnd.2.html

$./client_mpi $MPIPORT test 100000 8184
Registered new event type: ID=1 name='BatteryError' description='UnexpectedShutDown'
  ID            Name                     Description      Count        Sum
   1    BatteryError              UnexpectedShutDown     100000  818400000
Elapsed time: 0.608234 seconds
number of report IPC messages received 100000
throughput is 164410.318094 report IPC messages per second
throughput is 1345.534043 MB/second

$ ./client_mpi $MPIPORT test 100000 8185
Opened IPC mailbox queue number 90.
Sending an IPC message to register new event type 1 with name BatteryError and description UnexpectedShutDown
msgsnd: Invalid argument
Can't send IPC message.

* Throughput is 1739.621365 MB/second

* I am using radius for this experiment. 

* Statistics
    Registered new event type: ID=1 name='BatteryError' description='UnexpectedShutDown'
    ID            Name                     Description      Count        Sum
    1    BatteryError              UnexpectedShutDown     100000  818400000
    Elapsed time: 0.470447 seconds
    number of report IPC messages received 100000
    throughput is 212563.705342 report IPC messages per second
    throughput is 1739.621365 MB/second
*******************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

// Every SystemV IPC message needs to be a struct that starts with a long
// integer, followed by whatever other data you want. For the toy event-logging
// system, we will use one field to tell the server what operation to do, a
// second field to hold the event type ID, and a third field to hold other
// information (like the name and description when creating a new event type).
//
// NOTE: If you change this struct, you need to change it in client_mpi.c too.
struct ipcmsg {
    // The first field must be of type "long", as required by SystemV IPC.
    long msgtype;    // IPC message type (1 = register, 2 = report, 3 = reset, etc.)
    int eventid;     // the event type ID
    char data[0];    // other data (zero or more bytes)
};
// NOTE: the data array is declared here as an array of length zero. In the code
// below, the actual size may be zero (if there is no data for the message) but
// it will often be somze be some larger size.

// A "message" is everything in the above struct.
// For a message carrying n bytes of "other data", the total message size is:
#define MSG_SIZE(n) (sizeof(struct ipcmsg) + (n))

// The "payload" is everything in the above struct except the required first
// long integer.
// For a message carrying n bytes of "other data", the "payload sizeC" is:
#define MSG_PAYLOAD_SIZE(n) (sizeof(struct ipcmsg) - sizeof(long) + (n))

// The maximum message currently used is for "register" operation, which contains
// at most 16 bytes for the name (up to 15 characters plus a space at the end),
// and 32 bytes for the description (up to 31 characters plus a NUL at the end),
// plus the eventid, which should be a total of 16+32+4, less than 64 total.
#define MAX_MSG_SIZE (10000000)
#define MAX_MSG_PAYLOAD_SIZE (MAX_MSG_SIZE - sizeof(long))


// This struct holds information and statistics for one event type.
struct event_stats {
    char *name;
    char *description;
    int count;
    int sum;
};


// Global variables
struct event_stats stats[1024]; // table of info about all possible events
int q = -1; // identifier for the IPC mailbox queue
int reported = 0;
int indexof = 0;


// Print stats about all events
void print_stats() {
    printf("%4s %15s %31s %10s %10s\n", "ID", "Name", "Description", "Count","Sum");
    for (int i = 0; i < 1024; i++) {
        if (stats[i].name == NULL)
            continue;
        printf("%4d %15s %31s %10d %10d\n", i, stats[i].name, stats[i].description, stats[i].count, stats[i].sum);
    }
}


// This function gets invoked whenever the user presses Control-C.
void cleanup(int s) {

    // Remove the IPC mailbox queue.
    if (q >= 0) {
        if (msgctl(q, IPC_RMID, NULL) < 0) {
            perror("msgctl");
            printf("Can't remove IPC mailbox queue.\n");
            exit(1);
        }
        printf("Removed IPC mailbox queue.\n");
    }

    // Print a friendly message then exit.
    printf("Final event statistics...\n");
    print_stats();
    exit(1); 
}


// Register a new event type by setting the name and description. The data array
// should contain a name and a description, separated by a space and terminated
// with a NUL character.
void register_event_type(int eventid, int datasize, char *data) {
    if (eventid < 0 || eventid >= 1024) {
        printf("ERROR: can't register event ID %d\n", eventid);
        return;
    }
    if (data[datasize-1] != '\0') {
        printf("ERROR: can't register event ID %d, data is missing NUL terminator\n", eventid);
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
    stats[eventid].count = 0;
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
    struct timespec t_end;
    struct timespec t_start;

    if (argc != 2) {
        printf("usage: %s <mailbox_num>\n", argv[0]);
        printf("  You can use any positive number for the mailbox number\n");
        printf("  but it must be unique to you (if another person has already\n");
        printf("  created that mailbox queue, you won't be able to).\n");
        exit(1);
    }
    key_t key = atoi(argv[1]);

    // Initialize the event table to all zeros
    for (int i = 0; i < 1024; i++) {
        stats[i].name = NULL;
        stats[i].description = NULL;
        stats[i].count = -1;
        stats[i].count = -1;
    }

    // Create (or open) the mailbox queue.
    q = msgget(key, IPC_CREAT | 0660);
    if (q < 0) {
        perror("msgget");
        printf("Can't create IPC mailbox queue.\n");
        exit(1);
    }
    printf("Created IPC mailbox queue number %d.\n", key);

    printf("Waiting to receive IPC messages.\n");
    struct ipcmsg *m = (struct ipcmsg *)malloc(MAX_MSG_SIZE);
    while(1) {
        int desired_msgtype = 0; // 0 here means "any"
        int recv_flags = 0;
        int msgsize = msgrcv(q, m, MAX_MSG_PAYLOAD_SIZE, desired_msgtype, recv_flags);
        if (msgsize < 0) {
            perror("msgrecv");
            printf("Can't receive IPC message.\n");
            exit(1);
        }

        int datasize = msgsize - MSG_PAYLOAD_SIZE(0);

        // if (datasize > 0)
        //     printf("Received IPC message: msgsize=%d msgtype=%ld eventid=%d with %d bytes of data\n",
        //             msgsize, m->msgtype, m->eventid, datasize);
        // else
        //     printf("Received IPC message: msgsize=%d msgtype=%ld eventid=%d with no data\n",
        //             msgsize, m->msgtype, m->eventid);

        if (m->msgtype == 1) {
            register_event_type(m->eventid, datasize, m->data); // register event type
        } else if (m->msgtype == 2) {
            if(reported == 0)
            {
                clock_gettime(CLOCK_MONOTONIC, &t_start);
            }
            while(indexof < datasize)
            {
                stats[m->eventid].sum += m->data[indexof];
                indexof++;
            }
            stats[m->eventid].count++; // report event occurrence
            reported++;
            indexof = 0;
        } else if (m->msgtype == 3) {
            stats[m->eventid].count = 0; // reset event counter
        } else if(m->msgtype == 4) { 
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            print_stats(); // print statics about all events
            int seconds = t_end.tv_sec - t_start.tv_sec;
            int nanoseconds = t_end.tv_nsec - t_start.tv_nsec;
            double t = seconds + nanoseconds / 1e9;
            printf("Elapsed time: %0.6f seconds\n", t);
            printf("number of report IPC messages received %i\n", reported);
            printf("throughput is %f report IPC messages per second\n", reported/t);
            printf("throughput is %f MB/second\n", (stats[m->eventid].sum/1000000.0)/t);
            stats[m->eventid].sum = 0;
            reported = 0;
        } else {
            printf("Sorry, I don't know what to do for msgtype %ld.\n", m->msgtype);
        }
    }

    printf("All done!\n");
    cleanup(0);
    return 0;
}

