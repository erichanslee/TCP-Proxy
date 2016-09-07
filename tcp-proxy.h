#include "header.h"
struct sockaddr_in remote_addr; /* The address of the target server */
struct connection fdarray[MAX_CONN_HIGH_WATERMARK];





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
	int size;
	if(size = recv(origin_fd, buf, BUF_SIZE, 0)){
		if(size < -1){
			fprintf(stderr, "Receiving error %s\n", strerror(errno));
			close(origin_fd);
			close(destination_fd);
			return 0;
		}
		if(size == 0){
			close(origin_fd);
			close(destination_fd);
		}
		else{
			if(sendall(destination_fd, buf, size) < 0){
				fprintf(stderr, "Sending error %s\n", strerror(errno));
				return 0;
			}
		}
	}
}

int start_proxy(int client_fd, int server_fd){
	printf("Staring Proxy Forwarding...");
	void *client_buf = malloc(BUF_SIZE);
	void *server_buf = malloc(BUF_SIZE);
	int size, maxfd;



	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	maxfd = max(client_fd, server_fd) + 1;

	while(1){
			//todo: need to buffer
		FD_ZERO(&readfds);
		FD_SET(client_fd, &readfds);
		FD_SET(server_fd, &readfds);
		select(maxfd, &readfds, NULL, NULL, &tv);
		
		if(FD_ISSET(client_fd, &readfds)){
			forward(client_fd, server_fd, client_buf);
		}
		
		if(FD_ISSET(server_fd, &readfds)){
			forward(server_fd, client_fd, server_buf);
		}
		
		
	}
}


// Function to pass into ptrheads creation
void * ThreadTask(void *thread_arg){
	int threadidx = (int)thread_arg;
	printf("Hello from Thread %d!\n", threadidx);
	int i, client_fd, server_fd;
	while(1){
		for(i = threadidx; i < MAX_CONN_HIGH_WATERMARK; i +=MAX_THREAD_NUM){
			client_fd = fdarray[i].client_fd;
			server_fd = fdarray[i].server_fd;
			if( (client_fd != -1) && (server_fd != -1) ){
				printf("hello\n");
				start_proxy(client_fd, server_fd);
			}
		}

	}

		
}

void __loop(int proxy_fd)
{
	struct sockaddr_in client_addr;
	socklen_t addr_size;
	int client_fd, server_fd;
	struct client *client;
	int cur_thread=0;
	struct worker_thread *thread;
	char client_hname[MAX_ADDR_NAME+1];
	char server_hname[MAX_ADDR_NAME+1];
	pthread_t threads[MAX_THREAD_NUM];
	int i, idx;

	for(i = 0; i < MAX_CONN_HIGH_WATERMARK; i++){
		fdarray[i].client_fd = -1;
		fdarray[i].server_fd = -1;
	}

	for(i = 0; i < MAX_THREAD_NUM; i++){
		int rc = pthread_create(&threads[i], NULL, ThreadTask, (void *)i);
	}

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

		printf("Server Socket Found...\n");

		if (connect(server_fd, (struct sockaddr *) &remote_addr, 
			sizeof(struct sockaddr_in)) <0) {
			if (errno != EINPROGRESS) {
				fprintf(stderr, "connect error %s\n", strerror(errno));
				close(client_fd);
				close(server_fd);
				continue;
			}		
		}

		printf("Server Connected...\n");

		fdarray[idx].client_fd = client_fd;
		fdarray[idx].server_fd = server_fd;

		
		/*
		start_proxy(fdarray[idx].client_fd, fdarray[idx].server_fd);
		close(client_fd);
		close(server_fd);
		*/
	}
}
