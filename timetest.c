#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void){
char buffer [500];

struct tm *current;
                    time_t now;
                    time(&now);
                    current = localtime(&now);
                    sprintf(buffer, "%i-%i-%i %i:%i:%i\n\0", current->tm_year+1900, current->tm_mon, current->tm_mday, current->tm_hour, current->tm_min, current->tm_sec);

                    printf("%s", buffer);

}
