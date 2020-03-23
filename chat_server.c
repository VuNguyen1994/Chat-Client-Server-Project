/* Skeleton code for the server side code. 
 * 
 * Compile as follows: gcc -o chat_server chat_server.c -std=c99 -Wall -lrt
 *
 * Author: Naga Kandasamy
 * Date created: January 28, 2020
 *
 * Student/team name: FIXME
 * Date created: FIXME  
 *
 */

#define _POSIX_C_SOURCE 1 // FOR sigjmp_buf
#define _GNU_SOURCE  // For SA_RESTART

#include <signal.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "msg_structure.h"
#include <signal.h>
#include <setjmp.h>

/* FIXME: Establish signal handler to handle CTRL+C signal and 
 * shut down gracefully. 
 */
#define TRUE 1
#define FALSE 0
#define CTRLC 2020

static void alarm_handler (int); /* Signal handler to catch the ALRM signal */
static void ctrl_c_handler (int); /* Signal handler to catch CTRL+C signal */
static sigjmp_buf env;
struct sigaction action;
int client_fd;
char client_fifo[CLIENT_FIFO_NAME_LEN];
int n_clients = 0;
char usernames[100][USER_NAME_LEN];

int 
main (int argc, char **argv) 
{
    /* Set up signal handlers to catch SIGALRM and SIGINT */
    action.sa_handler = alarm_handler;
    action.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &action, NULL);

    signal (SIGINT, ctrl_c_handler); 

    int ret;
    ret = sigsetjmp (env, TRUE);
    switch (ret) {
        case 0:
            /* Returned from explicit sigsetjmp call */
            break;

        case CTRLC:
            printf ("\nSIGINT received\n");
            unlink (SERVER_FIFO);
            printf ( "Cleaning up and exiting safely...\n");
            exit (EXIT_SUCCESS);
    }

    /* Create a well-known FIFO and open it for reading. The server 
     * must be run before any of its clients so that the server FIFO exists by the 
     * time a client attempts to open it. The server's open() blocks until the first client 
     * opens the other end of the server FIFO for writing. 
     * */
    umask (0);
    if (mkfifo (SERVER_FIFO, S_IRUSR | S_IWUSR | S_IWGRP) == -1 \
        && errno != EEXIST) {
        perror ("mkfifo");
        exit (EXIT_FAILURE);
    }
   int server_fd = open (SERVER_FIFO, O_RDONLY);
   if (server_fd == -1) {
       perror ("open server fd");
       exit (EXIT_FAILURE);
   }

   /* Open the server's FIFO once more, this time for writing. This will not block 
    * since the FIFO has already been opened for reading. This operation is done 
    * so that the server will not see EOF if all clients close the write end of the 
    * FIFO.  
    */
   int dummy_fd = open (SERVER_FIFO, O_WRONLY);
   if (dummy_fd == -1) {
       perror ("open dummy_fd");
       exit (EXIT_FAILURE);
   }

   /* Ignore the SIGPIPE signal. This way if the server attempts to write to a client 
    * FIFO that doesn't have a reader, rather than the kernel sending it the SIGPIPE 
    * signal (which will by default terminate the server), it receives an EPIPE error 
    * from the write () call. 
    */
   if (signal (SIGPIPE, SIG_IGN) == SIG_ERR) {
       perror ("signal");
       exit (EXIT_FAILURE);
   }

   /* Loop that reads and responds to each incoming client request. To send the 
    * response, the server constructs the name of the client FIFO and then opens the 
    * FIFO for writing. If the server encounters an error in opening the client FIFO, 
    * it abandons that client's request and moves on to the next one.
    */
    struct client_msg req; //request
    struct server_msg resp; //response
    alarm (5); /* Set our alarm to ring at the specified interval */ 
    
   while (1) {
        if (read (server_fd, &req, sizeof (struct client_msg)) < 0) {
           fprintf (stderr, "SERVER: Error reading request; discarding \n");
           continue;
        }

        // Connect client to server
        if (req.control == CONTROL_CONNECTED){
            if (n_clients >= 100){
                printf("Server full. Server can connect to only 100 clients\n");
                continue;
            } 
            printf("Client %s connected to server.\n", req.user_name);
            strcpy(usernames[n_clients], req.user_name);
            n_clients++;
            continue;
        // Disconnect client from server
        } else if (req.control == CONTROL_DISCONNECTED) {
            // Copy the last username to the position that being deleted
             for (int i = 0; i < n_clients; i++){
                if (strcmp(req.user_name, usernames[i]) == 0) {
                    strcpy(usernames[i], "\0"); // in case i = 0 and n_clients = 1
                    strcpy(usernames[i], usernames[n_clients-1]);
                    strcpy(usernames[n_clients-1], "\0");
                    n_clients--;
                    printf("Client %s disconnected from server.\n", req.user_name);
                    break;
                }
            }
            continue;
        }
         
       /* Send the response to the client and close FIFO */
        if (req.broadcast == 1){
            strcpy (resp.msg, req.msg);
            strcpy (resp.sender_name, req.user_name);
            /* Construct the name of the client FIFO previously created by the client and 
            * open it for writing. Broadcast, have to loop to all clienfifo */
            for (int i = 0; i< n_clients; i++){
                if (strcmp(usernames[i], req.user_name) != 0) { // Don't send the message to the requester itself
                    snprintf (client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, usernames[i]/*(long)req.client_pid*/);
                    client_fd = open (client_fifo, O_WRONLY);
                    if (client_fd == -1) {    /* Open failed on the client FIFO. Give up and move on. */
                        printf ("SERVER: Error opening client fifo %s \n", client_fifo);
                        continue;
                    }
                    if (write (client_fd, &resp, sizeof (struct server_msg)) != sizeof (struct server_msg))
                        fprintf (stderr, "Error writing to client FIFO %s \n", client_fifo);
                    
                    printf("Send broadcast message '%s' to client %s.\n", resp.msg, usernames[i]);
                    if (close (client_fd) == -1)
                        fprintf (stderr, "Error closing client FIFO %s \n", client_fifo);
                    }
            }
        }
        else if (req.broadcast == 0)
        {
        /* Construct the name of the client FIFO previously created by the client and 
        * open it for writing. */
            strcpy(resp.msg, req.msg);
            strcpy(resp.sender_name, req.user_name);
            snprintf (client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, req.priv_user_name/*(long)req.client_pid*/);
            client_fd = open (client_fifo, O_WRONLY);
            if (client_fd == -1) {
                fprintf(stderr, "Cannot connect to client %s!\n", req.priv_user_name);
            } else {
                if (write (client_fd, &resp, sizeof (struct server_msg)) != sizeof (struct server_msg)) 
                    fprintf (stderr, "Error writing to client FIFO %s \n", client_fifo);
                printf("Send private message '%s' to client %s:\n", resp.msg, req.priv_user_name);
                if (close (client_fd) == -1)
                    fprintf (stderr, "Error closing client FIFO %s \n", client_fifo);
            }

        }

   }
    exit (EXIT_SUCCESS);
}


/* The user-defined signal handler for SIGALRM */
static void 
alarm_handler (int sig)
{
    // Send 'heartbeat' message to clients
    printf("Send ping message to %d clients\n", n_clients);
    for (int i = 0; i< n_clients; i++){
        snprintf (client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, usernames[i]/*(long)req.client_pid*/);
        client_fd = open (client_fifo, O_WRONLY);
        if (client_fd == -1) {    /* Open failed on the client FIFO. Give up and move on. */
            printf ("SERVER: Error opening client fifo %s \n", client_fifo);
            continue;
        }
        if (write (client_fd, &hearbeat, sizeof (struct server_msg)) != sizeof (struct server_msg))
        fprintf (stderr, "Error writing to client FIFO %s \n", client_fifo);
        if (close (client_fd) == -1)
            fprintf (stderr, "Error closing client FIFO %s \n", client_fifo);
    }
    sigaction(SIGALRM, &action, NULL); /* Restablish handler for next occurrence */
    alarm(5); 
    return;
}

/* The user-defined signal handler for SIGINT */
static void 
ctrl_c_handler (int sig)
{
    siglongjmp (env, CTRLC);
}