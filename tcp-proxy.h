#include "header.h"
#include <pthread.h>



int build_fd(int threadidx, fd_set *readfds, fd_set *writefds){
    int i, maxfd = 0;
    for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM) {
        int client_fd = fdarray[i].client_fd;
        int server_fd = fdarray[i].server_fd;
        if(client_fd != -1) {
            maxfd = max(client_fd, maxfd);
            FD_SET(client_fd, readfds);
            FD_SET(client_fd, writefds);
        }
        if(server_fd != -1) {
            maxfd = max(server_fd, maxfd);
            FD_SET(server_fd, readfds);
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
 modified to loop NUM_SEND times instead of while looping in case sending is too slow*/
int sendall(int destination_fd, struct buffer *conn_buffer, int len)
{
    void * buf = conn_buffer->buf;
	int total = 0;
	int bytesleft = len;
	int n, i;
	for(i = 0; i < NUM_SENDS; i ++) {
		n = send(destination_fd, buf+total, bytesleft, 0);
		if (n == -1) { break; }
		total += n;
		bytesleft -= n;
	}
    /* shift buffer since my circular array implementation wasn't working */
    memcpy(conn_buffer->buf, conn_buffer->buf + total, bytesleft);
    conn_buffer->buf_pointer = bytesleft;
    conn_buffer->time_since_last_send = 0;
	return 0;
}

void process_connection(int threadidx, fd_set *readfds, fd_set *writefds){
    clock_t start, end;
    start = clock();
    int i, size, flag;
    for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM){
        int client_fd = fdarray[i].client_fd;
        int server_fd = fdarray[i].server_fd;
        struct buffer *client_buf = &fdarray[i].client_buf;
        struct buffer *server_buf = &fdarray[i].server_buf;
        if (FD_ISSET(client_fd, readfds) && !buf_isfull(client_buf)) {
            size = buffer_recv(client_fd, server_fd, client_buf);

            /* Check for timeout */
            end = clock();
            client_buf->time_since_last_send += (float)(end - start) / CLOCKS_PER_SEC;

            /* Prune if necessary */
            if((size <= 0) || (client_buf->time_since_last_send > TIMEOUT))
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(server_fd, readfds) && !buf_isfull(server_buf)) {
            size = buffer_recv(server_fd, client_fd, server_buf);
            end = clock();
            server_buf->time_since_last_send += (float)(end - start) / CLOCKS_PER_SEC;
            if((size <= 0) || (client_buf->time_since_last_send > TIMEOUT))
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(client_fd, writefds)) {
            flag = sendall(server_fd, client_buf, client_buf->buf_pointer);
            if(flag == -1)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(server_fd, writefds)) {
            flag = sendall(client_fd, server_buf, server_buf->buf_pointer);
            if(flag == -1)
                prune_fds(client_fd, server_fd);
        }
    }
}


int start_proxy(int threadidx){
    fd_set readfds;
    fd_set writefds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    int maxfd = build_fd(threadidx, &readfds, &writefds);
    struct timeval tv;
    tv.tv_usec = 0;
    tv.tv_sec = 1;
    int MyConnectionCounter = -1;
    while(1){
        /* Block if no work */
        if(CUR_NUM_CONNECTIONS < threadidx + 1){
            printf("No Work, Thread %d going to sleep.\n", threadidx);
            pthread_cond_wait(&cond_isempty[threadidx], &mutexes[threadidx]);
            printf("Thread %d Woken Up!\n", threadidx);
        }

        /* Yes, I know it's expensive to rebuild it every iteration */
        maxfd = build_fd(threadidx, &readfds, &writefds);

        /* select restarts every second to check for new connections */
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
        fdarray[i].client_buf.time_since_last_send = 0;
        fdarray[i].server_buf.time_since_last_send = 0;
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
