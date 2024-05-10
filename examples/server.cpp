#include <stdio.h>
#include "rtmp.h"

int main(int argc, char * argv[])
{
    const char * url = "rtmp://live-play.newsharing.com/live/yangshipin3m?key=val";
    rtmp_context_t * ctx = os_rtmp_alloc(url, RTMP_CLIENT_WRITE);
    if (NULL == ctx)
        return 1;

    os_rtmp_parse_url(ctx);

    os_rtmp_connect(ctx);

    return 0;
}
