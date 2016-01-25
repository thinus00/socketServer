/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define PORT "3490"  // the port users will be connecting to
#define MAXDATASIZE 100 // max number of bytes we can get at once
#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            const char *filename1 = "data.log";
            const char *filename2 = "data2.log";
            int lastmin = 0;
            int lasthour = 0;
            char buf[MAXDATASIZE];
            int numbytes = 0;
            while(1) {
                if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
                    perror("recv");
                    break;
                }

                if (numbytes == 0) {
                    printf("\nConnection closed by client\n");
                    break;
                }
                buf[numbytes] = '\0';

                printf("%s: received '%s'\n",s,buf);

                char buffer [500];
                //char *buffer;
                struct tm *current;
                time_t now;
                time(&now);
                current = localtime(&now);
                sprintf(buffer, "%i-%i-%i %i:%i:%i,%s,%s\n\0", current->tm_year+1900, current->tm_mon, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec, s, buf);
                if (lasthour == 0) {
                    lasthour = current->tm_hour;
                }
                printf("%s", buffer);
                FILE *file1;
                file1 = fopen(filename1, "a");
                if (file1) {
                    fwrite(buffer, sizeof(char), strlen(buffer), file1);
                    fclose (file1);
                }

                if ((current->tm_min > (lastmin + 5)) && (current->tm_hour == lasthour)) {
                    printf("Writing file2\n");
                    FILE *file2;
                    file2 = fopen(filename2, "a");
                    if (file2) {
                        fwrite(buffer, sizeof(char), strlen(buffer), file2);
                        fclose (file2);
                    }
                    lastmin = current->tm_min;
                    lasthour = current->tm_hour;

                    if ((lastmin + 5) > 59) {
                       lastmin = lastmin - 60;
                       lasthour = current->tm_hour + 1;
                    }
                    printf("lastmin: %i\n", lastmin);
                }

            }

            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}
