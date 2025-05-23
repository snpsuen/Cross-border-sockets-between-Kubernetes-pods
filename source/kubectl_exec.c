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
#include <sys/wait.h>

#define NSLEN 8
#define MINLEN 64
#define MEDLEN 128
#define MAXLEN 256
#define BUFLEN 1024

int set_con_ns(char* container);

int set_con_ns(char* container) {
        char command[MAXLEN], containerid[MINLEN], nspath[MINLEN];
        char* nslist[NSLEN] = {"cgroup", "ipc", "mnt", "net", "pid", "time", "user", "uts"}
        FILE* fout;
        int i, cpid, cfd;
        
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

        for (i = 0; i < NSLEN; i++) {
                memset(nspath, 0, sizeof(nspath));
                sprintf(nspath, "/proc/%d/ns/%s", cpid, nslist[i]);

                printf("Calling cfd = open(%s, O_RDONLY | O_CLOEXEC) ...", nspath);
                cfd = open(nspath, O_RDONLY | O_CLOEXEC);
                if (cfd == -1)
                        err(EXIT_FAILURE, "open %s", nspath);
                printf("Open %s = %d\n", nspath, cfd);

                if (setns(cfd, 0) == -1)       /* Join that namespace */
                        err(EXIT_FAILURE, "setns from %s", nspath);

                close(cfd);
        }

        return 0;
}

int main(int argc, char* argv[]) {
        int i;
        pid_t child;
        char* token;
        char container[MINLEN], execstring[MAXLEN];
        char* execargv[MAXLEN];

        memset(container, 0, sizeof(container));
        printf("Enter the container name: ");
        fgets(container, sizeof(container), stdin);
        container[strlen(container) - 1] = 0;

        set_con_ns(container);
        if ((child = fork()) < 0)
                perror("fork error");

        if (child == 0) {
                memset(execstring, 0, sizeof(execstring));
                printf("Enter the command to exec in %s: ", execstring);
                fgets(execstring, sizeof(execstring), stdin);
                execstring[strlen(execstring) - 1] = 0;

                i = 0;
                token = strtok(execstring, " ");
                while (token != NULL) {
                        execargv[i] = token;
                        token = strtok(NULL, " ");
                        i++;
                }
                execargv[i] = NULL;

                if (execvp(execargv[0], execargv) < 0) {
                        perror("execvp error");
                        exit(1);
                }
        }

        if (child > 0)
                waitpid(child, NULL, 0);

        return 0;
}
