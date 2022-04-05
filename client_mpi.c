// client_mpi.c
//Student Name: Apruva Gandhi
// Simple message-passing client using SystemV style IPC. 
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
// NOTE: If you change this struct, you need to change it in server_mpi.c too.
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


int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: %s <mailbox_num> [ register <id> <name> <desc> | reset <id> | print | report <id> | test <count> <size> ]\n", argv[0]);
        printf("  You can use any positive number for the mailbox number\n");
        printf("  but it must be unique to you (if another person has already\n");
        printf("  created that mailbox queue, you won't be able to).\n");
        exit(1);
    }

    key_t key = atoi(argv[1]);
    //to do that 
    int index = 0;
    int reported = 0;

    // Open the mailbox queue, but don't create one if it doesn't exist yet.
    int q = msgget(key, 0);
    if (q == -1) {
        perror("msgget");
        printf("Can't open IPC mailbox queue.\n");
        exit(1);
    }
    printf("Opened IPC mailbox queue number %d.\n", key);

    if (!strcmp(argv[2], "register")) {
        if (argc != 6) {
            printf("you must provide event id, name, and description");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        char *name = argv[4];
        char *desc = argv[5];
        int n = strlen(name) + 1 + strlen(desc) + 1;
        printf("Sending an IPC message to register new event type %d with name %s and description %s\n",
                eventid, name, desc);
        struct ipcmsg *m = (struct ipcmsg *)malloc(MSG_SIZE(n));
        m->msgtype = 1; // 1 means "register"
        m->eventid = eventid;
        sprintf(m->data, "%s %s", name, desc);
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(n), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        free(m);
    } else if (!strcmp(argv[2], "report")) {
        if (argc != 4) {
            printf("you must provide event id");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        //printf("Sending an IPC message to report occurrence of event type %d\n", eventid);
        struct ipcmsg *m = (struct ipcmsg *)malloc(MSG_SIZE(0));
        m->msgtype = 2; // 2 means "report"
        m->eventid = eventid;
        // m->data is not used here
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(0), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        free(m);
    } else if (!strcmp(argv[2], "reset")) {
        if (argc != 4) {
            printf("you must provide event id");
            exit(1);
        }
        int eventid = atoi(argv[3]);
        printf("Sending an IPC message to reset statistics for event type %d\n", eventid);
        struct ipcmsg *m = (struct ipcmsg *)malloc(MSG_SIZE(0));
        m->msgtype = 3; // 3 means "reset"
        m->eventid = eventid;
        // m->data is not used here
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(0), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        free(m);
    } else if(!strcmp(argv[2], "print")) {
        printf("Sending an IPC message to print statistics for each registered type of event\n");
        struct ipcmsg *m = (struct ipcmsg *)malloc(MSG_SIZE(0));
        m->msgtype = 4; // 4 means "print statistics"
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(0), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        free(m);
    } else if(!strcmp(argv[2], "test")) {
        if (argc != 5) {
            printf("you must provide number of counts for reporting\n");
            printf("you must provide size for data to send with each report\n");
            exit(1);
        } else if(argv[3] <= 0 || argv[4] < 0) {
            printf("You must enter count to be greater than 0 and size to be greater than or equal to 0");
            exit(1);
        }
        int count = atoi(argv[3]);
        int datasize = atoi(argv[4]);
        //registering new event
        int eventid = 1;
        char *name = "BatteryError";
        char *desc = "UnexpectedShutDown";
        int n = strlen(name) + 1 + strlen(desc) + 1;
        printf("Sending an IPC message to register new event type %d with name %s and description %s\n",
                eventid, name, desc);
        struct ipcmsg *m = (struct ipcmsg *)malloc(MSG_SIZE(n+datasize));
        m->msgtype = 1; // 1 means "register"
        m->eventid = eventid;
        sprintf(m->data, "%s %s", name, desc);
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(n), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        //reporting event
        m->msgtype = 2; // 2 means "report"
        m->eventid = eventid;
        while(index < datasize)
        {
            m->data[index] = 1;
            index++;
        }
        while(reported < count)
        {
            //printf("Sending an IPC message to report occurrence of event type %d\n", eventid);
            // m->data is not used here
            if (msgsnd(q, m, MSG_PAYLOAD_SIZE(datasize), 0) < 0) {
                perror("msgsnd");
                printf("Can't send IPC message.\n");
                exit(1);
            }
            reported++;
        }
        
        //sending a print message
        printf("Sending an IPC message to print statistics for each registered type of event\n");
        m->msgtype = 4; // 4 means "print statistics"
        if (msgsnd(q, m, MSG_PAYLOAD_SIZE(0), 0) < 0) {
            perror("msgsnd");
            printf("Can't send IPC message.\n");
            exit(1);
        }
        free(m);
    } else {
        printf("Sorry, I don't know how to do '%s'\n", argv[2]);
        exit(1);
    }
    
    // Sadly, there is no built-in SystemV IPC way for the server to reply. So
    // we will just assume that the message eventually gets there and the server
    // successfully performs the requested operation without any errors.

    printf("All done!\n");
    return 0;
}

