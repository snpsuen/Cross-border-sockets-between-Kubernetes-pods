#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/param.h>
#include <errno.h>

#define MINLEN 64
#define MEDLEN 128
#define MAXLEN 256
#define BUFLEN 1024

int main(int argc, char* argv[]) {
	int sock;
	struct sockaddr_in server;
	struct hostent* host;
	char errormsg[MAXLEN], request[MAXLEN], buffer[BUFLEN];
	char *ptr, *myhost, *myport;
	int remaining, written, readin, endofsock;

	myhost = argv[1];
	myport = argv[2];

	if (myhost == NULL)
		myhost = "localhost";

	if (myport == NULL)
		myport = "8080";

	if ((host = gethostbyname(myhost)) == (struct hostent*)NULL) {
		snprintf(errormsg, sizeof(errormsg), "Error: gethostbyname() errno = %d", errno);
		perror(errormsg);
		exit(1);
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		snprintf(errormsg, sizeof(errormsg), "Error: socket() errno = %d", errno);
		perror(errormsg);
		exit(2);
	}

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	memcpy(&server.sin_addr, host->h_addr, host->h_length);
	server.sin_port = htons(atoi(myport));

	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
		snprintf(errormsg, sizeof(errormsg), "Error: connect() errno = %d", errno);
		perror(errormsg);
		exit(3);
	}

	endofsock = 0;
	while (!endofsock) {
		memset(request, 0, sizeof(request));
		printf("Enter remote command or \"quit\":>> ");
		fgets(request, sizeof(request), stdin);
		/* request[strlen(request) - 1] = 0; */

		if (strncmp(request, "quit", 4) == 0)
			break;

		ptr = request;
		remaining = strlen(request);
		while (remaining) {
			if ((written = write(sock, ptr, remaining)) < 0) {
				snprintf(errormsg, sizeof(errormsg), "Error: write() errno = %d", errno);
				perror(errormsg);
				exit(4);
			}
			printf("Wrote %d bytes to socket ...\n", written);
			ptr += written;
			remaining -= written;
		}

		while (1) {
			if ((readin = read(sock, buffer, sizeof(buffer))) < 0) {
				snprintf(errormsg, sizeof(errormsg), "Error: read() errno = %d", errno);
				perror(errormsg);
				exit(5);
			}

			if (readin == 0) {
				endofsock = 1;
				break;
			}

			buffer[readin] = 0;
			if (fputs(buffer, stdout) < 0) {
				snprintf(errormsg, sizeof(errormsg), "Error: fputs() errno = %d", errno);
				perror(errormsg);
				exit(6);
			}

			if (strncmp(buffer + (readin - 27), "\nOutput from request ends.\n", 27) == 0)
				break;

		}
	}

	close(sock);
	return 0;
}