#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
extern char *tzname[];

int main()
{
    time_t now;
    struct tm *sp;

    // === ПЕРВАЯ СТРОКА: PDT (America/Los_Angeles с авто-DST) ===
    setenv("TZ", "America/Los_Angeles", 1);
    tzset();

    (void) time( &now );
    printf("%s", ctime( &now ) );   // здесь будет PDT (если сейчас лето)

    // === ВТОРАЯ СТРОКА: PST (всегда зимнее) ===
    setenv("TZ", "PST8PST", 1);
    tzset();

    sp = localtime(&now);
    printf("%d/%d/%02d %d:%02d %s\n",
        sp->tm_mon + 1, sp->tm_mday,
        sp->tm_year + 1900, sp->tm_hour,
        sp->tm_min, tzname[sp->tm_isdst]);

    exit(0);
}
