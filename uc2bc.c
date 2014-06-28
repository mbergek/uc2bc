/*
* The MIT License (MIT)
* 
* Copyright (c) 2008 Martin Bergek
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

void
usage(char *cmd)
{
	// Get the name of the executable
	char* name = strrchr(cmd, '/') ? strrchr(cmd, '/') + 1 : cmd;
	
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s [options]\n", name);	
	fprintf(stderr, "\n");	
	fprintf(stderr, "Options:\n");	
	fprintf(stderr, "  --listen-port -i [port]       # The port to listen for incoming packets on\n");	
	fprintf(stderr, "  --broadcast-address -b [addr] # The local broadcast address\n");	
	fprintf(stderr, "  --broadcast-port -o [port]    # The port to use for outgoing packets\n");	
	fprintf(stderr, "  --source-address -s [addr]    # The source for authorised packets\n");	
	fprintf(stderr, "\n");
	fprintf(stderr, "Description:\n");
	fprintf(stderr, "  Listens for UDP packets on the specified port on all\n");
	fprintf(stderr, "  local interfaces. Any received packet is sent on the\n");
	fprintf(stderr, "  broadcast address on the specified port. The listen\n");
	fprintf(stderr, "  and broadcast ports must be different. If the source\n");
	fprintf(stderr, "  address is specified, incoming packets must come from\n");		
	fprintf(stderr, "  that address in order to be forwarded.\n");		
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "  %s -i 5000 -b 192.168.0.255 -o 5001\n", name);
	fprintf(stderr, "\n");
	exit(1);
}

int
listenon(unsigned int port)
{
	int fd;
	int one = 1;

	struct sockaddr_in sin;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror("socket() failed");
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	(void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
	(void)setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const char *)&one, sizeof(one));

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin))) {		
		perror("bind() failed");
		close(fd);
		exit(1);
	}

	return fd;
}

void
nozombies()
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDWAIT;

	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction() failed");
		exit(1);
	}
}

void
process(char *buf, ssize_t len, char *ip, unsigned int port, unsigned int sport, char* destination, unsigned int dport)
{
	int sockfd;
	int broadcast=1;
	struct sockaddr_in recvaddr;
	int numbytes;
	
	switch (fork()) {
	case -1:
		perror("fork() failed");
		break;

	case 0: /* Child */
		nozombies();

		/* Send packet */		

		if((sockfd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
			perror("sockfd");
			exit(1);
		}

		if((setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast)) == -1) {
			perror("setsockopt - SO_SOCKET ");
			exit(1);
		}

		recvaddr.sin_family = AF_INET;
		recvaddr.sin_port = htons(dport);
		recvaddr.sin_addr.s_addr = inet_addr(destination);
		memset(recvaddr.sin_zero, '\0', sizeof recvaddr.sin_zero);
		numbytes = sendto(sockfd, buf, len , 0, (struct sockaddr *)&recvaddr, sizeof recvaddr);

		close(sockfd);
		
		exit(1);

	default: /* Parent */
		break;
	}
}

void
getpacket(int fd, unsigned int sport, char* destination, unsigned int dport, char* source)
{
	char buf[65536];
	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	ssize_t len;

	len = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sin, &sinlen);
	if (len == -1) {
		perror("recvfrom() failed");
	} else {
		char *ip;
		unsigned int port;
		if (sinlen != sizeof(sin)) {
			ip = "?";
			port = 0;
		} else {
			ip = inet_ntoa(sin.sin_addr);
			port = ntohs(sin.sin_port);
		}
		
		if(source) {
			if(strcmp(source, ip)) {
				fprintf(stderr, "Wrong sender [%s] - [%s]\n", source, ip);	
				return;
			}
		}
		
		process(buf, len, ip, port, sport, destination, dport);
	}
}

int
main(int argc, char *argv[])
{
	unsigned int sport = 0;
	unsigned int dport = 0;
	char *destination = NULL;
	char *source = NULL;
	int fd;

	int i = 1;
	while(i<argc) {
		
		if(!strcmp(argv[i], "--listen-port") || !strcmp(argv[i], "-i")) {
			sport = atoi(argv[i+1]);
		}
		else if(!strcmp(argv[i], "--broadcast-port") || !strcmp(argv[i], "-o")) {
			dport = atoi(argv[i+1]);
		}
		else if(!strcmp(argv[i], "--broadcast-address") || !strcmp(argv[i], "-b")) {
			destination = argv[i+1];
		}
		else if(!strcmp(argv[i], "--source-address") || !strcmp(argv[i], "-s")) {
			source = argv[i+1];
		}

		i += 2;
	}

	if (sport < 1 || sport > 65535)
		usage(argv[0]);		
	if (dport < 1 || dport > 65535)
		usage(argv[0]);			

	/* Workaround to avoid loop */
	if(dport == sport)
		usage(argv[0]);

	nozombies();

	fd = listenon(sport);
	for (;;) {
		getpacket(fd, sport, destination, dport, source);
	}

	/* NOTREACHED */
	return 0;
}
