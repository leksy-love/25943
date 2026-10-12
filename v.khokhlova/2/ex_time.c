#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    time_t now = time(NULL);
    char buf[64];
    struct tm *lt;
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();
    lt = localtime(&now);
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y %Z", lt);
    printf("%s\n", buf);

    setenv("TZ", "PST8", 1);   
    tzset();
    lt = localtime(&now);
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y %Z", lt);
    printf("%s\n", buf);

    return 0;
}
