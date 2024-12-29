#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <sched.h>
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
    int sock;
    struct sockaddr_in server;
    char *ptr, *myfront, *myserver, *myport, *myline;
    int remaining, readin, written, length, fd;
    pid_t child, pid;
    char errormsg[MAXLEN], request[MAXLEN], buffer[BUFLEN], container[MINLEN], nspath[MINLEN];
    FILE* fout;

    myfront = argv[1];
    myserver = argv[2];
    myport = argv[3];
    myline = argv[4];

    if (myfront == NULL)
         myserver = "frontender";

    if (myserver == NULL)
         myserver = "127.0.0.1";

    if (myport == NULL)
            myport = "9000";

    if (myline == NULL)
        myline = "And there you are ...";

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        snprintf(errormsg, sizeof(errormsg), "Error: socket() errno = %d", errno);
        perror(errormsg);
        exit(1);
    }

    pid = getpid();
    memset(nspath, 0, sizeof(nspath));
    snprintf(nspath, sizeof(nspath), "/proc/%d/ns/net", pid);
    fd = open(nspath, O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        err(EXIT_FAILURE, "open");

    set_con_ns(myfront);
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
    inet_aton(myserver, &server.sin_addr);
    server.sin_port = htons(atoi(myport));

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        snprintf(errormsg, sizeof(errormsg), "Error: connect() errno = %d", errno);
        perror(errormsg);
        exit(2);
    }

    memset(request, 0, sizeof(request));
    snprintf(request, sizeof(request), "%s\n", myline);

    ptr = request;
    remaining = strlen(request);
    while (remaining) {
        if ((written = write(sock, ptr, remaining)) < 0) {
            snprintf(errormsg, sizeof(errormsg), "Error: write() errno = %d", errno);
            perror(errormsg);
            exit(3);
        }
        printf("Wrote %d bytes to socket ...\n", written);
        ptr += written;
        remaining -= written;
    }

    while (1) {
        if ((readin = read(sock, buffer, sizeof(buffer))) < 0) {
            snprintf(errormsg, sizeof(errormsg), "Error: read() errno = %d", errno);
            perror(errormsg);
            exit(4);
        }

        if (readin == 0)
            break;

        buffer[readin] = 0;
        if (fputs(buffer, stdout) < 0) {
            snprintf(errormsg, sizeof(errormsg), "Error: fputs() errno = %d", errno);
            perror(errormsg);
            exit(5);
        }

        if (buffer[strlen(buffer) - 1] = '\n')
            break;
    }

    close(sock);
    return 0;
}
