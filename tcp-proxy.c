/*
 * Copyright (C) 2014 Ki Suh Lee (kslee@cs.cornell.edu)
 * 
 * TCP Proxy skeleton.
 * 
 */

#include "tcp-proxy.h"


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

int main(int argc, char **argv)
{
	char *remote_name;
	struct sockaddr_in proxy_addr;
	unsigned short local_port, remote_port;
	struct hostent *h;
	int arg_idx = 1, proxy_fd;
	
	if (argc != 4)
	{
		fprintf(stderr, "Usage %s <remote-target> <remote-target-port> "
			"<local-port>\n", argv[0]);
		exit(1);
	}

	remote_name = argv[arg_idx++];
	remote_port = atoi(argv[arg_idx++]);
	local_port = atoi(argv[arg_idx++]);

	/* Lookup server name and establish control connection */
	if ((h = gethostbyname(remote_name)) == NULL) {
		fprintf(stderr, "gethostbyname(%s) failed %s\n", remote_name, 
			strerror(errno));
		exit(1);
	}
	memset(&remote_addr, 0, sizeof(struct sockaddr_in));
	remote_addr.sin_family = AF_INET;
	memcpy(&remote_addr.sin_addr.s_addr, h->h_addr_list[0], sizeof(in_addr_t));
	remote_addr.sin_port = htons(remote_port);
	
	/* open up the TCP socket the proxy listens on */
	if ((proxy_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		fprintf(stderr, "socket error %s\n", strerror(errno));
		exit(1);
	}
	/* bind the socket to all local addresses */
	memset(&proxy_addr, 0, sizeof(struct sockaddr_in));
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_addr.s_addr = INADDR_ANY; /* bind to all local addresses */
	proxy_addr.sin_port = htons(local_port);
	if (bind(proxy_fd, (struct sockaddr *) &proxy_addr, 
			sizeof(proxy_addr)) < 0) {
		fprintf(stderr, "bind error %s\n", strerror(errno));
		exit(1);
	}

	listen(proxy_fd, MAX_LISTEN_BACKLOG);

	printf("Waiting for Connection...\n");

	__loop(proxy_fd);

	return 0;
}
