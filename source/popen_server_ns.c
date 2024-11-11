#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

#define MINLEN 64
#define MEDLEN 128
#define MAXLEN 256
#define BUFLEN 1024

int set_con_ns(char* container);

int set_con_ns(char* container) {
	char command[256], containerid[64], nspath[64];
	FILE* fout;
	int cpid, cfd;
	
	memset(command, 0, sizeof(command));
	sprintf(command, "crictl ps | grep %s | cut -d\' \' -f1", container);
	fout = popen(command, "r");
	if (fscanf(fout, "%s", containerid) != 1) {
		printf("Can't find container ID for container %s \n", container);
		exit(1);
	}
	pclose(fout);

        memset(command, 0, sizeof(command));
	sprintf(command, "crictl inspect --output go-template --template \'{{.info.pid}}\' %s", containerid);
	fout = popen(command, "r");
	if (fscanf(fout, "%d", &cpid) != 1) {
		printf("Can't find container process ID for container %s \n", container);
		exit(1);
	}
	pclose(fout);

        memset(nspath, 0, sizeof(nspath));
	sprintf(nspath, "/proc/%d/ns/net", cpid);
	cfd = open(nspath, O_RDONLY | O_CLOEXEC);
	if (cfd == -1)
	    err(EXIT_FAILURE, "open");

        if (setns(cfd, 0) == -1)       /* Join that namespace */
	    err(EXIT_FAILURE, "setns");
	
	close(cfd);
	return 0;
}

int main(int argc, char* argv[]) {
	int sock, new, length;
	int remaining, readin, in_sock, in_req, fd;
	pid_t child, pid;
	struct sockaddr_in server, client;
	char* ptr;
	char errormsg[MAXLEN], request[MAXLEN], buffer[BUFLEN], container[MINLEN], nspath[MINLEN];
	FILE* fout;

        if (argc > 1)
		sprintf(container, "%s", argv[1]);
	else
		sprintf(container, "%s", "curlybox");

        pid = getpid();
        memset(nspath, 0, sizeof(nspath));
	sprintf(nspath, "/proc/%d/ns/net", pid);
	fd = open(nspath, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
	    err(EXIT_FAILURE, "open");
	
        set_con_ns(container);
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		snprintf(errormsg, sizeof(errormsg), "Error: socket() errno = %d", errno);
		perror(errormsg);
		exit(1);
	}
	
	if (setns(fd, 0) == -1)
	    err(EXIT_FAILURE, "setns");
	
	close(fd);

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(0);
	length = sizeof(server);

	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
		snprintf(errormsg, sizeof(errormsg), "Error: bind() errno = %d", errno);
		perror(errormsg);
		exit(2);
	}

	if (getsockname(sock, (struct sockaddr*)&server, &length) < 0) {
		snprintf(errormsg, sizeof(errormsg), "Error: bind() errno = %d", errno);
		perror(errormsg);
		exit(3);
	}
	printf("Server is listening to port no. %d \n", ntohs(server.sin_port));

	if (listen(sock, 5) < 0) {
		sprintf(errormsg, "Error: listen() errno = %d", errno);
		perror(errormsg);
		exit(4);
	}

	while (1) {
		length = sizeof(client);
		if ((new = accept(sock, (struct sockaddr*)&client, &length)) < 0) {
			snprintf(errormsg, sizeof(errormsg), "Error: listen() errno = %d", errno);
			perror(errormsg);
			exit(5);
		}

		if (fork() == 0) {
			close(sock);
			in_sock = 1;

			while (in_sock) {
				memset(request, 0, sizeof(request));
				ptr = request;
				remaining = sizeof(request);
				in_req = 1;

				while (in_req) {
					if ((readin = read(new, ptr, remaining)) < 0) {
						snprintf(errormsg, sizeof(errormsg), "Error: read () errno = %d", errno);
						perror(errormsg);
						in_sock = 0;
						break;
					}
					printf("Read %d bytes from socket ...\n", readin);

					if (readin == 0) {
						in_req = 0;
						in_sock = 0;
						break;
					}

					if (ptr[readin - 1] == '\n') {
						ptr[readin - 1] = 0;
						in_req = 0;
						break;
					}

					ptr += readin;
					remaining -= readin;

					if (remaining == 0) {
						in_req = 0;
						break;
					}
				}

				if (in_sock == 0)
						break;

				printf("Server received request %s ...\n", request);
				strncat(request, " 2>&1", MINLEN);
				if ((fout = popen(request, "r")) == NULL) {
					snprintf(errormsg, sizeof(errormsg), "Error: popen () errno = %d", errno);
					perror(errormsg);
					continue;
				}

				memset(buffer, 0, sizeof(buffer));
				snprintf(buffer, sizeof(buffer), "Output from request %s:\n", request);
				if (write(new, buffer, strlen(buffer)) < 0) {
					snprintf(errormsg, sizeof(errormsg), "Error: write() errno = %d", errno);
					perror(errormsg);
					in_sock = 0;
					break;
				}

				memset(buffer, 0, sizeof(buffer));
				while(fgets(buffer, sizeof(buffer), fout) != NULL) {
					if (write(new, buffer, strlen(buffer)) < 0) {
						snprintf(errormsg, sizeof(errormsg), "Error: write () errno = %d", errno);
						perror(errormsg);
						in_sock = 0;
						break;
					}
				}

				memset(buffer, 0, sizeof(buffer));
				snprintf(buffer, sizeof(buffer), "\nOutput from request ends.\n");
				if (write(new, buffer, strlen(buffer)) < 0) {
					snprintf(errormsg, sizeof(errormsg), "Error: write() errno = %d", errno);
					perror(errormsg);
					in_sock = 0;
					break;
				}

			}

			close(new);
			exit(0);

		}

		close(new);
	}

	close(sock);
	return 0;

}
