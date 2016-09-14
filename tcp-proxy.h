#include "header.h"
#include <pthread.h>

int build_fd(int threadidx, fd_set *readfds, fd_set *writefds){
    int i, maxfd = 0;
    for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM) {
        int client_fd = fdarray[i].client_fd;
        int server_fd = fdarray[i].server_fd;
        if(client_fd != -1 && server_fd != -1) {
            maxfd = max(client_fd, maxfd);
            maxfd = max(server_fd, maxfd);
            FD_SET(client_fd, readfds);
            FD_SET(server_fd, readfds);
            FD_SET(client_fd, writefds);
            FD_SET(server_fd, writefds);
        }
    }
    maxfd = maxfd + 1;
    return maxfd;
}

int buffer_recv(int origin_fd, int destination_fd, struct buffer buf){
    int size = recv(origin_fd, buf.buf, BUF_SIZE, 0);
    if(size == 0 ){
        close(origin_fd);
        close(destination_fd);
    }
    if(size < -1){
        fprintf(stderr, "Receiving error %s\n", strerror(errno));
        close(origin_fd);
        close(destination_fd);
    }
    return size;
}

void prune_fds(int client_fd, int server_fd){
    int idx = findval_client(fdarray, MAX_CONN_HIGH_WATERMARK, client_fd);
    fdarray[idx].client_fd = -1;
    fdarray[idx].server_fd = -1;
    pthread_mutex_lock(&tot_conn_mutex);
    TOTAL_CONNECTIONS--;
    pthread_mutex_unlock(&tot_conn_mutex);
}

/* sendall partially taken from Beej's Guide to Network Programming to handle partial sends
	... Changed for syntatic clarity and functionality*/
int sendall(int destination_fd, struct buffer conn_buffer, int len)
{
    void * buf = conn_buffer.buf;
	int total = 0;
	int bytesleft = len;
	int n;
	while(total < len) {
		n = send(destination_fd, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}
	return n==-1?-1:0;
}

void process_connection(int threadidx, fd_set *readfds, fd_set *writefds){
    int i, size = -1;
    for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM){
        int client_fd = fdarray[i].client_fd;
        int server_fd = fdarray[i].server_fd;
        struct buffer client_buf = fdarray[i].client_buf;
        struct buffer server_buf = fdarray[i].server_buf;
        if (FD_ISSET(client_fd, readfds)) {
            size = buffer_recv(client_fd, server_fd, client_buf);
            if(size <= 0)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(server_fd, readfds)) {
            size = buffer_recv(server_fd, client_fd, server_buf);
            if(size <= 0)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(client_fd, readfds)) {
            sendall(server_fd, client_buf, size);

        }
        if (FD_ISSET(server_fd, writefds)) {
            sendall(client_fd, server_buf, size);
        }
    }
}
// TODO: Implement Complete Buffering

int start_proxy(int threadidx){
    fd_set readfds;
    fd_set writefds;
    struct timeval tv;
    while(1){
        int maxfd = 0;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        tv.tv_usec = 0;
        tv.tv_sec = 1;
        maxfd = build_fd(threadidx, &readfds, &writefds);
        /* select restarts every 5 seconds to check for new connections */
        select(maxfd, &readfds, &writefds, NULL, &tv);
        process_connection(threadidx, &readfds, &writefds);
	}
}
void Initstuff(){
	int i;
	for(i = 0; i < MAX_CONN_HIGH_WATERMARK; i++){

		fdarray[i].client_fd = -1;
		fdarray[i].server_fd = -1;
        fdarray[i].client_buf.buf = malloc(BUF_SIZE);
        fdarray[i].server_buf.buf = malloc(BUF_SIZE);
        fdarray[i].client_buf.buf_pointer = 0;
        fdarray[i].server_buf.buf_pointer = 0;

	}
    for (i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
        pthread_mutex_lock(&mutexes[i]);
    }
    for(i = 0; i < MAX_THREAD_NUM; i++){
        pthread_create(&threads[i], NULL, ThreadTask, (void *)i);
    }

}

// Function to pass into pthreads creation
void * ThreadTask(void *thread_arg){
	int threadidx = (int)thread_arg;

    printf("Thread %d Spawned and Waiting...\n", threadidx);
    pthread_mutex_lock(&mutexes[threadidx]);
    printf("Thread %d Woken Up!\n", threadidx);
    start_proxy(threadidx);
	pthread_mutex_unlock(&mutexes[threadidx]);

}
