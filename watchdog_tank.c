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

int main(void)
{
    // Connect to the database 

    MYSQL      *MySQLConRet;
    MYSQL      *MySQLConnection = NULL;

    const char *hostName = "localhost";
    const char *userId   = "espuser";
    const char *password = "M@t0rb1k3pi";
    const char *DB       = "tanklevels";

    MySQLConnection = mysql_init( NULL );

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

    int alarmed[100] = {0};
    char buffer [500];
    while(1) {  // main accept() loop

        sprintf(buffer, "SELECT * FROM tanklevel_rt\0");
        printf(buffer);
        printf("\n");

        if (mysql_query(MySQLConnection, buffer))
        {
            printf("Error %u: %s\n", mysql_errno(MySQLConnection), mysql_error(MySQLConnection));
        }
        else
        {
            time_t now;
            time(&now);  /* get current time; same as: now = time(NULL)  */
            struct tm timeinfo;
            timeinfo = *localtime (&now);
            printf("Now: %4d-%2d-%2d %2d:%2d:%2d\n", timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

            printf("selected tanklevel_rt\n");
            MYSQL_RES *res; /* holds the result set */
            res = mysql_store_result(MySQLConnection);

            // get the number of the columns
            int num_fields = mysql_num_fields(res);

            MYSQL_ROW row;
            // Fetch all rows from the result
            while ((row = mysql_fetch_row(res)))
            {
                // Print all columns
                int id = atoi(row[0]);
                char dateBuf[50];
                sprintf(dateBuf,"%s",row[1]);
                printf("ID:%d; Timestamp:%s\n",id,dateBuf);

		struct tm tm1  = { 0 };
		//2016-01-17 22:39:02
                sscanf(dateBuf,"%4d-%2d-%2d %2d:%2d:%2d",&tm1.tm_year,&tm1.tm_mon,&tm1.tm_mday,&tm1.tm_hour,&tm1.tm_min,&tm1.tm_sec);

		tm1.tm_year = tm1.tm_year-1900;
		tm1.tm_mon = tm1.tm_mon-1;
		tm1.tm_isdst = -1;
		mktime(&tm1);

		double seconds = difftime(mktime(&timeinfo),mktime(&tm1));
		printf("DiffTime: %.0f\n\n", seconds);
                if (seconds > 300)
                {
                    if (alarmed[id] == 0)
                    {
                        printf("ALARM for %d\n", id);
                        alarmed[id] = 1;
sprintf(buffer,"curl -u o.JnTIFQ2hlubf79J6MEdQ9KfvJ77iwDoC: https://api.pushbullet.com/v2/pushes -d type=note -d title='Tank alarm ID %d' -d body='Alarm on ID:%d. Down from %s for %.0f seconds'", id, id, row[1], seconds);
			printf("%s\n",buffer);
			system(buffer);
                    }
                    else
                    {
                        printf("ALARM! %d already alarmed\n", id);
                    }
                }
                else
                {
                    if (alarmed[id] == 1)
                    {
                        printf("Disable alarm for %d\n", id);
sprintf(buffer,"curl -u o.JnTIFQ2hlubf79J6MEdQ9KfvJ77iwDoC: https://api.pushbullet.com/v2/pushes -d type=note -d title='Tank alarm removed ID %d' -d body='Alarm on ID:%d removed'", id, id);
                        printf("%s\n",buffer);
                        system(buffer);
                    }
                    alarmed[id] = 0;
                }
            }
            printf("sleeping for 5 seconds...\n");
            sleep(5);
        }
    }

    return 0;
}
