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

#include <mysql.h>

#define PORT "3490"  // the port users will be connecting to
#define MAXDATASIZE 100 // max number of bytes we can get at once
#define BACKLOG 10     // how many pending connections queue will hold

typedef unsigned long long uint64;

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

    // Connect to the database

    MYSQL      *MySQLConRet;
    MYSQL      *MySQLConnection = NULL;

    const char *hostName = "localhost";
    const char *userId   = "espuser";
    const char *password = "M@t0rb1k3pi";
    const char *DB       = "tanklevels";

    MySQLConnection = mysql_init( NULL );

//    try
//    {
        MySQLConRet = mysql_real_connect( MySQLConnection,hostName,userId,password,DB,0,NULL,0 );

        if ( MySQLConRet == NULL )
        {
            printf("error connecting to SQL");
            printf(mysql_error(MySQLConnection));
            exit;
        }
        printf("MySQL Connection Info: %s \n", mysql_get_host_info(MySQLConnection));
        printf("MySQL Client Info: %s \n", mysql_get_client_info());
        printf("MySQL Server Info: %s \n", mysql_get_server_info(MySQLConnection));
//    }
//    catch ( FFError e )
//    {
//        printf("%s\n",e.Label.c_str());
//        return 1;
//    }


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
            //const char *filename1 = "data.log";
            //const char *filename2 = "data2.log";
            char buf[MAXDATASIZE];
            int numbytes = 0;
            int done = 0;
            while(done == 0) {
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
                if ((strncmp(buf, "GET", 3) == 0) || (strncmp(buf, "q", 1) == 0)){
                    sprintf(buffer, "SELECT * FROM tanklevel_rt\0");
                    printf(buffer);
                    printf("\n");

                    if (mysql_query(MySQLConnection, buffer))
                    {
                        printf("Error %u: %s\n", mysql_errno(MySQLConnection), mysql_error(MySQLConnection));
                    }
                    else
                    {
                        char sbuffer [5000];
			char sbuf [500];
                        printf("selected tanklevel_rt\n");
                        MYSQL_RES *res; /* holds the result set */
			res = mysql_store_result(MySQLConnection);

			// get the number of the columns
                        int num_fields = mysql_num_fields(res);

                        MYSQL_ROW row;
                        sbuffer[0] = '\0';
                        // Fetch all rows from the result
                        while ((row = mysql_fetch_row(res)))
                        {
                           sbuf[0] = '\0';
                           // Print all columns
                           int i = 0;
                           for(i = 0; i < num_fields; i++)
                           {
                               // Make sure row[i] is valid!
                               if(row[i] != NULL)
                               {
                                    sprintf(sbuf, "%s%s,", sbuf, row[i]);
                               }
                               else
                               {
                                    sprintf(sbuf, "%sNULL,", sbuf);
                               }
                               // Also, you can use ternary operator here instead of if-else
                               // cout << row[i] ? row[i] : "NULL" << endl;
                           }
                           sprintf(sbuffer, "%s%s\n\0", sbuffer, sbuf);
                           sprintf(sbuf,"%s\0", sbuf);
                           printf("%s\n", sbuf);
                        }
                        //if (send(new_fd, "<h1>Hello, world!</h1>", 22, 0) == -1) {
                        char sbuffer2 [5000];
			if (strncmp(buf, "GET", 3) == 0) {
                           printf("%s\n", sbuffer);
                           sprintf(sbuffer2, "<html><body>%s</body></html>\0", sbuffer);
                        }
                        else {
                           printf("%s\n", sbuffer);
                           sprintf(sbuffer2, "%s\0", sbuffer);
                        }
                        if (send(new_fd, sbuffer2, strlen(sbuffer2), 0) == -1) {
                           printf("Error sending");
                        }
                        else {
                           printf("Sent: %s\n",sbuffer2);
                        }
                        done = 1;
                    }
                }
                else {
                    struct tm *current;
                    time_t now;
                    time(&now);
                    current = localtime(&now);
                    char id;
                    id = s[strlen(s)-1];
                    sprintf(buffer, "%i-%i-%i %i:%i:%i,%s,%s-%c\n\0", current->tm_year+1900, current->tm_mon+1, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec, s, buf, id);

                    printf("%s", buffer);
                    //FILE *file1;
                    //file1 = fopen(filename1, "a");
                    //if (file1) {
                    //    fwrite(buffer, sizeof(char), strlen(buffer), file1);
                    //    fclose (file1);
                    //}

                    int buf_int = atoi(buf);
                    if ((buf_int < 20000) && (buf_int > 0)) {

struct timeval tv;
    gettimeofday(&tv, NULL);

uint64 start = tv.tv_usec;
start /= 1000;
start += (tv.tv_sec * 1000);
                        sprintf(buffer, "UPDATE tanklevel_rt SET value=%s, timestamp=\"%i-%i-%i %i:%i:%i\" WHERE tankid = %c\0", buf, current->tm_year+1900, current->tm_mon+1, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec, id);
                        printf("%c-%s",id,buffer);
                        printf("\n");

                        if (mysql_query(MySQLConnection, buffer))
                        {
                            printf("%c-Error %u: %s\n", id,mysql_errno(MySQLConnection), mysql_error(MySQLConnection));
                        }
                        else
                        {
gettimeofday(&tv, NULL);
uint64 end = tv.tv_usec;
end /= 1000;
end += (tv.tv_sec * 1000);

                                printf("%c-Updated DB (%dms)\n", id, end-start);

                        }
                    } else {
                        printf("Invalid value %d\n", buf_int);
                    }
                }
            }

            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}
