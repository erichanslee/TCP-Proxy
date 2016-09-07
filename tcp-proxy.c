/*
 * Copyright (C) 2014 Ki Suh Lee (kslee@cs.cornell.edu)
 * 
 * TCP Proxy skeleton.
 * 
 */

#include "tcp-proxy.h"



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
