/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include <mysql.h>

int main(void)
{
    int yes=1;
    int rv;

    // Connect to the database

    MYSQL      *MySQLConRet;
    MYSQL      *MySQLConnection = NULL;

    const char *hostName = "localhost";
    const char *userId   = "tempuser";
    const char *password = "M@t0rb1k3pi";
    const char *DB       = "temps";

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

    char command[100];
    sprintf(command,"vcgencmd measure_temp");

    printf("starting main loop...\n");

    while(1) {  // main accept() loop

	printf("Measuring temp...\n");

        FILE* pipe = popen(command, "r");		//Send the command, popen exits immediately
	if (!pipe)
		return 0;

	char buffer[128];
	char result[500];
	while(!feof(pipe))						//Wait for the output resulting from the command
	{
	    if(fgets(buffer, 128, pipe) != NULL)
            {
	        strcpy(result, buffer);
            }
	}
	pclose(pipe);

        printf("recevied: %s\n", result);

        struct tm *current;
        time_t now;
        time(&now);
        current = localtime(&now);
        char id;
        id = '0';
	char buffer2[500];
	char buf[10];

	//temp=41.2'C
	char * pch;
        pch = strchr(result,'=');
	if ((pch-result,1) > 0)
        {
            char * pch2 = strchr(pch,'\'');
            sprintf(buf,"%s",pch+1);
            buf[pch2-pch-1] = '\0';

            double val = atof(buf);
            int val2 = val * 10;

            buffer[0] = '\0';
            sprintf(buffer, "UPDATE temp_rt SET value=%d, timestamp=\"%i-%i-%i %i:%i:%i\" WHERE tempid = %c\0", val2, current->tm_year+1900, current->tm_mon+1, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec, id);
            printf(buffer);
            printf("\n");

            if (mysql_query(MySQLConnection, buffer))
            {
                printf("Error %u: %s\n", mysql_errno(MySQLConnection), mysql_error(MySQLConnection));
            }
            else
            {
                printf("Updated DB");
            }


           sleep(10);
       }

    }

    return 0;
}
