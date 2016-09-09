#include "header.h"
#include <pthread.h>

/* Global Variables */
struct sockaddr_in remote_addr; /* The address of the target server */
struct connection fdarray[MAX_CONN_HIGH_WATERMARK]; /* array of file descriptors */
pthread_mutex_t count_mutex[MAX_THREAD_NUM];
pthread_t threads[MAX_THREAD_NUM];
int MAX_CONNECTIONS = 0; /* Optimization variable to track maximum # of connections */

// Searches array for first instance of val and returns its idx. Otherwise,  returns size + 1
int findval(struct connection * array, int size, int val){
	int i;
	for(i = 0; i < size; i++){
		if(array[i].client_fd == val && array[i].server_fd == val){
			return i;
		}
	}
	return size + 1;
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



int forward(int origin_fd, int destination_fd, void *buf){
    ssize_t size = 0;
    size = recv(origin_fd, buf, BUF_SIZE, 0);
    if(size < -1){
        fprintf(stderr, "Receiving error %s\n", strerror(errno));
        close(origin_fd);
        close(destination_fd);
    }
    if(size == 0){
        close(origin_fd);
        close(destination_fd);
    }
    else{
        sendall(destination_fd, buf, size);
    }
	return 0;
}

int start_proxy(int client_fd, int server_fd, void * client_buf, void * server_buf){
	int size, maxfd;

	fd_set readfds;
	struct timeval tv;

	maxfd = max(client_fd, server_fd) + 1;

	while(1){
        tv.tv_sec = 5;
        tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(client_fd, &readfds);
		FD_SET(server_fd, &readfds);
        /* select restarts every 5 seconds to check for new connections */
		select(maxfd, &readfds, NULL, NULL, &tv);
		if(FD_ISSET(client_fd, &readfds)){
			forward(client_fd, server_fd, client_buf);
		}
		
		if(FD_ISSET(server_fd, &readfds)){
			forward(server_fd, client_fd, server_buf);
		}
		
		
	}
}
void Initstuff(){
	int i;
	for(i = 0; i < MAX_CONN_HIGH_WATERMARK; i++){
		fdarray[i].client_fd = -1;
		fdarray[i].server_fd = -1;
	}

    for (i = 0; i < MAX_THREAD_NUM; i++) {
        pthread_mutex_init(&count_mutex[i], NULL);
        pthread_mutex_lock(&count_mutex[i]);
    }

    for(i = 0; i < MAX_THREAD_NUM; i++){
        pthread_create(&threads[i], NULL, ThreadTask, (void *)i);
    }

}

// Function to pass into pthreads creation
void * ThreadTask(void *thread_arg){
	int threadidx = (int)thread_arg;
    void *client_buf = malloc(BUF_SIZE);
    void *server_buf = malloc(BUF_SIZE);
    int i, client_fd, server_fd;


    printf("Thread %d Spawned and Waiting...\n", threadidx);
    pthread_mutex_lock(&count_mutex[threadidx]);
    printf("Thread %d Woken Up!\n", threadidx);

	while(1){
		for(i = threadidx; i < MAX_CONNECTIONS; i += MAX_THREAD_NUM){
			client_fd = fdarray[i].client_fd;
			server_fd = fdarray[i].server_fd;
			if( (client_fd != -1) && (server_fd != -1) ){
				start_proxy(client_fd, server_fd, client_buf, server_buf);
			}
		}
	pthread_mutex_unlock(&count_mutex[threadidx]);

	}
}
 // __loop done by main thread
void __loop(int proxy_fd)
{
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	int client_fd, server_fd;
	char client_hname[MAX_ADDR_NAME+1];
	char server_hname[MAX_ADDR_NAME+1];
	int idx;

	Initstuff();
    int threadcounter = -1;

	while(1) {

		idx = findval(fdarray, MAX_CONN_HIGH_WATERMARK, -1);


		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		addr_size = sizeof(client_addr);
		client_fd = accept(proxy_fd, (struct sockaddr *)&client_addr,
					&addr_size);
		if(client_fd == -1) {
			fprintf(stderr, "accept error %s\n", strerror(errno));
			continue;
		}

		// For debugging purpose
		if (getpeername(client_fd, (struct sockaddr *) &client_addr, &addr_size) < 0) {
			fprintf(stderr, "getpeername error %s\n", strerror(errno));
		}

		strncpy(client_hname, inet_ntoa(client_addr.sin_addr), MAX_ADDR_NAME);
		strncpy(server_hname, inet_ntoa(remote_addr.sin_addr), MAX_ADDR_NAME);

		// TODO: Disable following printf before submission
		printf("Connection proxied: %s:%d --> %s:%d\n",
				client_hname, ntohs(client_addr.sin_port),
				server_hname, ntohs(remote_addr.sin_port));

		// Connect to the server
		if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			fprintf(stderr, "socket error %s\n", strerror(errno));
			close(client_fd);
			continue;
		}


		if (connect(server_fd, (struct sockaddr *) &remote_addr, 
			sizeof(struct sockaddr_in)) <0) {
			if (errno != EINPROGRESS) {
				fprintf(stderr, "connect error %s\n", strerror(errno));
				close(client_fd);
				close(server_fd);
				continue;
			}		
		}

		printf("Connection Made...\n");

		fdarray[idx].client_fd = client_fd;
		fdarray[idx].server_fd = server_fd;
		if(MAX_CONNECTIONS < 256) MAX_CONNECTIONS++;
        if(threadcounter < 4){
            threadcounter++;
            /* Wake thread threadcounter */
            pthread_mutex_unlock(&count_mutex[threadcounter]);
        }


	}
}
