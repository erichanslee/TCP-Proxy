#include "header.h"
#include <pthread.h>

int buf_isfull(int fd){
    return 0;
}

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

void prune_fds(int client_fd, int server_fd){
    int idx = findval_client(fdarray, MAX_CONN_HIGH_WATERMARK, client_fd);
    fdarray[idx].client_fd = -1;
    fdarray[idx].server_fd = -1;
    pthread_mutex_lock(&tot_conn_mutex);
    CUR_NUM_CONNECTIONS--;
    pthread_mutex_unlock(&tot_conn_mutex);
    printf("Connection Finished!\n");
}

int buffer_recv(int origin_fd, int destination_fd, struct buffer *conn_buf){

    int size = recv(origin_fd, conn_buf->buf + conn_buf->buf_pointer, BUF_SIZE - conn_buf->buf_pointer, 0);
    conn_buf->buf_pointer += size;
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

/* sendall partially taken from Beej's Guide to Network Programming to handle partial sends
	... Changed for syntatic clarity and functionality*/
int sendall(int destination_fd, struct buffer *conn_buffer, int len)
{
    void * buf = conn_buffer->buf;
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
        struct buffer *client_buf = &fdarray[i].client_buf;
        struct buffer *server_buf = &fdarray[i].server_buf;
        if (FD_ISSET(client_fd, readfds) && !buf_isfull(client_fd)) {
            size = buffer_recv(client_fd, server_fd, client_buf);
            if(size <= 0)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(server_fd, readfds) && !buf_isfull(server_fd)) {
            size = buffer_recv(server_fd, client_fd, server_buf);
            if(size <= 0)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(client_fd, readfds) && !buf_isfull(client_fd)) {
            sendall(server_fd, client_buf, client_buf->buf_pointer);

        }
        if (FD_ISSET(server_fd, writefds) && !buf_isfull(server_fd)) {
            sendall(client_fd, server_buf, server_buf->buf_pointer);
        }
    }
}
// TODO: Implement Complete Buffering

int start_proxy(int threadidx){
    fd_set readfds;
    fd_set writefds;
    fd_set readfds_cpy;
    fd_set writefds_cpy;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&readfds_cpy);
    FD_ZERO(&writefds_cpy);
    int maxfd = build_fd(threadidx, &readfds, &writefds);
    struct timeval tv;
    int MyConnectionCounter = -1;
    while(1){
        pthread_mutex_lock(&mutexes[threadidx]);
        if(CUR_NUM_CONNECTIONS < threadidx + 1){
            printf("No Work, Thread going to sleep.\n");
            pthread_cond_wait(&cond_isempty[threadidx], &mutexes[threadidx]);
            printf("Thread %d Woken Up!\n", threadidx);
        }
        pthread_mutex_unlock(&mutexes[threadidx]);

        tv.tv_usec = 0;
        tv.tv_sec = 1;

        /* Rebuilt Connection Set if # Cons has changed */
        if(MyConnectionCounter != NET_CONNECTIONS_HANDLED){
            maxfd = build_fd(threadidx, &readfds, &writefds);
            MyConnectionCounter = NET_CONNECTIONS_HANDLED;
        }

        /* Copy fds to prevent rebuilding every loop */
        memcpy(&readfds_cpy, &readfds, sizeof(readfds_cpy));
        memcpy(&writefds_cpy, &writefds, sizeof(writefds_cpy));
        /* select restarts every second to check for new connections */
        select(maxfd, &readfds_cpy, &writefds_cpy, NULL, &tv);
        process_connection(threadidx, &readfds_cpy, &writefds_cpy);
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
        pthread_cond_init(&cond_isempty[i], NULL);
    }

    for(i = 0; i < MAX_THREAD_NUM; i++){
        pthread_create(&threads[i], NULL, ThreadTask, (void *)i);
    }
}

// Function to pass into pthreads creation
void * ThreadTask(void *thread_arg){
	int threadidx = (int)thread_arg;

    printf("Thread %d Spawned and Waiting...\n", threadidx);
    start_proxy(threadidx);
}
