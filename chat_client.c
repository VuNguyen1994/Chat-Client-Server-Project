/* Skeleton code for the client side code. 
 *
 * Compile as follows: gcc -o chat_client chat_client.c -std=c99 -Wall -lrt
 *
 * Author: Naga Kandsamy
 * Date created: January 28, 2020
 * Date modified:
 *
 * Student/team name: FIXME
 * Date created: FIXME 
 *
*/
#define _POSIX_C_SOURCE 1
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
#include <setjmp.h>

#define TRUE 1
#define FALSE 0
#define CTRLC 2020

static void alarm_handler (int); /* Signal handler to catch the ALRM signal */
static void ctrl_c_handler (int); /* Signal handler to catch CTRL+C signal */
static sigjmp_buf env;
struct sigaction action;
/*Init msg queuse with user_name and pid*/
int server_fd, client_fd;
struct client_msg req; //request
struct server_msg resp; //response
int pid; // child pid for read msg from server

void 
print_main_menu (void)
{
    printf ("\n'B'roadcast message\n");
    printf ("'P'rivate message\n");
    printf ("'E'xit\n");
    return;
}

void exit_client();

static char client_fifo[CLIENT_FIFO_NAME_LEN];

/* Exit handler for the program. */
static void 
remove_fifo (void)
{
    unlink (client_fifo);
}

int 
main (int argc, char **argv)
{
    action.sa_handler = alarm_handler;
    action.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &action, NULL);
    signal (SIGINT, ctrl_c_handler);
    // Handling middle crash
    int ret;
    ret = sigsetjmp (env, TRUE);
    switch (ret) {
    case 0:
        /* Returned from explicit sigsetjmp call */
        break;

    case CTRLC:
        // exit (EXIT_SUCCESS);
        printf ("\nSIGINT received\n");
        printf ( "Cleaning up and exiting safely...\n");
        req.control = CONTROL_DISCONNECTED;
        if (write (server_fd, &req, sizeof (struct client_msg)) != sizeof (struct client_msg)) {
            printf ("Cannot write to server\n");
            exit (EXIT_FAILURE);
        }
        if (atexit (remove_fifo) != 0) {
            perror ("atexit");
            exit (EXIT_FAILURE);
        }
        if (pid) {
            if(kill(pid, SIGKILL) == 0){
                printf("Client Exited!!\n");
            }
        }
        exit (EXIT_SUCCESS);
    }

    char user_name[USER_NAME_LEN];

    if (argc != 2) {
        printf ("Usage: %s user-name\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    strcpy (user_name, argv[1]); /* Get the client user name */

    /* FIXME: Connect to server */
    /* Create a FIFO using the template to be used for receiving 
     * the response from the server. This is done before sending the 
     * request to the server to make sure that the FIFO exists by the 
     * time the server attempts to open it and send the response message. 
     */
    umask (0);
    snprintf (client_fifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, user_name/*(long) getpid ()*/);
    printf ("User %s connecting to server \n", client_fifo);
    if (mkfifo (client_fifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST) {
        perror ("mkfifo");
        exit (EXIT_FAILURE);
    }
    
    req.client_pid = getpid();
    strcpy (req.user_name, user_name);
    // control flag here
    server_fd = open (SERVER_FIFO, O_WRONLY | O_NONBLOCK); // O_NONBLOCK to return immedately if server is not open yet!
    if (server_fd == -1) {
        printf ("Cannot open server FIFO %s\n", SERVER_FIFO);
        exit (EXIT_FAILURE);   
    }
    req.control = CONTROL_CONNECTED;
    strcpy(req.msg, "");
    if (write (server_fd, &req, sizeof (struct client_msg)) != sizeof (struct client_msg)) {
        printf ("Cannot write to server\n");
        exit (EXIT_FAILURE);
    }

    req.control = 0;
    /* Operational menu for client */
    char option;
    
    // Fork process to read from server
    if ((pid=fork())== 0){
        client_fd = open (client_fifo, O_RDONLY);
        if (client_fd == -1) {
            printf ("Cannot open FIFO %s for reading \n", client_fifo);
            exit (EXIT_FAILURE);
        }
        while(1) {
            /* Open our FIFO and read the response from the server. */
            int ret_val = read (client_fd, &resp, sizeof (struct server_msg));
            if (ret_val == -1) {
                fprintf (stderr, "Cannot read response from server \n");
                exit (EXIT_FAILURE);
            } else if (ret_val == sizeof (struct server_msg)) {
                if (strcmp(resp.sender_name, "") == 0 && strcmp(resp.msg, "") == 0) {
                    alarm(SERVER_PING_TIMEOUT); // Reset alarm here
                } else
                    printf ("Client %s said: %s \n", resp.sender_name, resp.msg);
            }
        }
    }

    while (1) {
        print_main_menu ();
        scanf(" %c", &option);
        getchar();
        switch (option) {
            case 'B':
                req.broadcast = 1;
                /*Get input msg*/
                printf("Enter broadcast message: ");
                scanf("%[^\n]", req.msg); 
                getchar(); //discard the last /n from buffer
                /*Write req msg to server*/
                if (write (server_fd, &req, sizeof (struct client_msg)) != sizeof (struct client_msg)) {
                    printf ("Cannot write to server\n");
                    exit (EXIT_FAILURE);
                }
                printf("Broad msg sent to server\n");
                break;

            case 'P':
                /* FIXME: Get name of private user and send the private 
                * message to server to be sent to private user */
                req.broadcast = 0;
                printf("Private msg to username: ");
                fscanf(stdin, "%s", req.priv_user_name);
                
                getchar(); //discard the last /n from private username buffer
                printf("Enter private message: ");
                scanf("%[^\n]", req.msg);
                getchar(); //discard the last /n from buffer     
                if (write (server_fd, &req, sizeof (struct client_msg)) != sizeof (struct client_msg)) {
                    printf ("Cannot write to server\n");
                    exit (EXIT_FAILURE);
                }
                printf("Private msg sent to server\n");
                break;

            case 'E':
                printf ("Chat client exiting\n");
                /* FIXME: Send message to server that we are exiting */
                /* Establish an exit handler so that when the client program exits, the FIFO is removed 
                * from the file system. 
                */
                req.control = CONTROL_DISCONNECTED;
                if (write (server_fd, &req, sizeof (struct client_msg)) != sizeof (struct client_msg)) {
                    printf ("Cannot write to server\n");
                    exit (EXIT_FAILURE);
                }
                if (atexit (remove_fifo) != 0) {
                    perror ("atexit");
                    exit (EXIT_FAILURE);
                }
                if (pid) {
                    if(kill(pid, SIGKILL) == 0){
                        printf("Client Exited!!\n");
                    }
                }
                exit (EXIT_SUCCESS);

            default:
                printf ("Unknown option\n");
                break;
                
        }
    }
}

/* The user-defined signal handler for SIGALRM */
static void 
alarm_handler (int sig)
{
    printf("Server crashed. Client Exiting... \n");
    if (atexit (remove_fifo) != 0) {
        perror ("atexit");
        exit (EXIT_FAILURE);
    }
    int parentid = getppid();
    if (parentid) {
        kill(parentid, SIGKILL);
    }
    exit (EXIT_SUCCESS);
    return;
}

/* The user-defined signal handler for SIGINT */
static void 
ctrl_c_handler (int sig)
{
    siglongjmp (env, CTRLC);
}
