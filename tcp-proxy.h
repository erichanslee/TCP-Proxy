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

void process_connection(int threadidx, fd_set *readfds, fd_set *writefds){
    int i, size, kill_fd_flag;
    for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM){
        int client_fd = fdarray[i].client_fd;
        int server_fd = fdarray[i].server_fd;
        void * client_buf = fdarray[i].client_buf;
        void * server_buf = fdarray[i].server_buf;
        // TODO: Close fds when forward returns 1
        if (FD_ISSET(client_fd, readfds)) {
            size = buffer_recv(client_fd, server_fd, client_buf);
            // TODO: Fix kill_flag
            if(kill_fd_flag == 1)
                prune_fds(client_fd, server_fd);
        }
        if (FD_ISSET(server_fd, readfds)) {
            size = buffer_recv(server_fd, client_fd, server_buf);
            if(kill_fd_flag == 1)
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

int buffer_recv(int origin_fd, int destination_fd, void *buf){
    int size = recv(origin_fd, buf, BUF_SIZE, 0);
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
int sendall(int destination_fd, char *buf, int len)
{
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
// TODO: Implement Complete Buffering

int start_proxy(int threadidx){
    fd_set readfds;
    fd_set writefds;
    struct timeval tv;
    while(1){
        int i, kill_fd_flag, size, maxfd = 0;
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
        fdarray[i].cbuf_pointer = 0;
        fdarray[i].sbuf_pointer = 0;
        fdarray[i].client_buf = malloc(BUF_SIZE);
        fdarray[i].server_buf = malloc(BUF_SIZE);
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
 // __loop done by main thread
void __loop(int proxy_fd)
{
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	int client_fd, server_fd;
	char client_hname[MAX_ADDR_NAME+1];
	char server_hname[MAX_ADDR_NAME+1];
	int idx = 0;
    struct sockaddr_in copyRemote = copy_remote_addr(remote_addr);
	Initstuff();
    int threadcounter = -1;

	while(1) {
        if (TOTAL_CONNECTIONS < MAX_CONN_HIGH_WATERMARK) {
            memset(&client_addr, 0, sizeof(struct sockaddr_in));
            addr_size = sizeof(client_addr);
            client_fd = accept(proxy_fd, (struct sockaddr *) &client_addr,
                               &addr_size);
            if (client_fd == -1) {
                fprintf(stderr, "accept error %s\n", strerror(errno));
                continue;
            }

            // For debugging purpose
            if (getpeername(client_fd, (struct sockaddr *) &client_addr, &addr_size) < 0) {
                fprintf(stderr, "getpeername error %s\n", strerror(errno));
            }

            strncpy(client_hname, inet_ntoa(client_addr.sin_addr), MAX_ADDR_NAME);
            strncpy(server_hname, inet_ntoa(copyRemote.sin_addr), MAX_ADDR_NAME);

            // TODO: Disable following printf before submission
            printf("Connection proxied: %s:%d --> %s:%d\n",
                   client_hname, ntohs(client_addr.sin_port),
                   server_hname, ntohs(copyRemote.sin_port));

            // Connect to the server
            if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                fprintf(stderr, "socket error %s\n", strerror(errno));
                close(client_fd);
                continue;
            }


            if (connect(server_fd, (struct sockaddr *) &copyRemote,
                        sizeof(struct sockaddr_in)) < 0) {
                if (errno != EINPROGRESS) {
                    fprintf(stderr, "connect error %s\n", strerror(errno));
                    close(client_fd);
                    close(server_fd);
                    continue;
                }
            }

            printf("Connection Made...\n");

            idx = findval(fdarray, MAX_CONN_HIGH_WATERMARK, -1);
            fdarray[idx].client_fd = client_fd;
            fdarray[idx].server_fd = server_fd;

            /* Update Optimization Counter */
            if (MAX_CONNECTIONS < 256) MAX_CONNECTIONS++;

            /* Update Total Connections */
            pthread_mutex_lock(&tot_conn_mutex);
            TOTAL_CONNECTIONS++;
            if(TOTAL_CONNECTIONS == MAX_CONN_HIGH_WATERMARK) printf("Maximum Number of Connections Reached!\n");
            pthread_mutex_unlock(&tot_conn_mutex);

            /* Wake threads if necessary */
            if (threadcounter < MAX_THREAD_NUM) {
                threadcounter++;
                pthread_mutex_unlock(&mutexes[threadcounter]);
            }

        }
    }
}
