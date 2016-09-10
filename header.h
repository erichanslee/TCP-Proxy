 // TCP Proxy header.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include "list.h"

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define MAX_LISTEN_BACKLOG 5
#define MAX_ADDR_NAME	32
#define ONE_K	1024

/* Data that can pile up to be sent before reading must stop temporarily */
#define MAX_CONN_BACKLOG	(8*ONE_K)
#define GRACE_CONN_BACKLOG	(MAX_CONN_BACKLOG / 2)

/* Watermarks for number of active connections. Lower to 2 for testing */
#define MAX_CONN_HIGH_WATERMARK	(256)
#define MAX_CONN_LOW_WATERMARK	(MAX_CONN_HIGH_WATERMARK - 1)

#define MAX_THREAD_NUM	4
#define BUF_SIZE 4096

struct connection{
	int client_fd;
	int server_fd;
};

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* ~~~~~~~~~~~~~~~~~~~~~~ GLOBAL VARIABLES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 struct sockaddr_in remote_addr; /* The address of the target server */
 struct connection fdarray[MAX_CONN_HIGH_WATERMARK]; /* array of file descriptors */
 pthread_mutex_t mutexes[MAX_THREAD_NUM];
 pthread_t threads[MAX_THREAD_NUM];
 pthread_mutex_t tot_conn_mutex; /* Mutex for TOTAL_CONNECTIONS */
 int TOTAL_CONNECTIONS = 0; /* Tracks total number # of connections */
 int MAX_CONNECTIONS = 0; /* Optimization variable to representing upper bound on # of connections */


 /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 /* ~~~~~~~~~~~~~~~~~~~~~~~ PROXY FUNCTIONS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
int sendall(int destination_fd, char *buf, int len);
int forward(int origin_fd, int destination_fd, void *buf);
//int start_proxy(int client_fd, int server_fd);
void * ThreadTask(void *thread_arg);
void __loop(int proxy_fd);



 /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 /* ~~~~~~~~~~~~~~~~~~~~~~ HELPER FUNCTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
 /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Searches array for first instance of val and returns its idx. Otherwise,  returns size + 1 */
 /* Note: Yes, there is some redundancy but I'd rather have them readable then have harder to understand code */
 int findval(struct connection * array, int size, int val){
     int i;
     for(i = 0; i < size; i++){
         if(array[i].client_fd == val && array[i].server_fd == val){
             return i;
         }
     }
     return size + 1;
 }

 int findval_server(struct connection * array, int size, int val){
     int i;
     for(i = 0; i < size; i++){
         if(array[i].server_fd == val){
             return i;
         }
     }
     return size + 1;
 }

 int findval_client(struct connection * array, int size, int val){
     int i;
     for(i = 0; i < size; i++){
         if(array[i].client_fd == val){
             return i;
         }
     }
     return size + 1;
 }

 struct sockaddr_in copy_remote_addr(struct sockaddr_in remote_addr){
     struct sockaddr_in copy;
     memcpy(&copy, &remote_addr, sizeof(copy));
 }