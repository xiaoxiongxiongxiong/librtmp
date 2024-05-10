#include "rtmp.h"
#include "log.h"
#include <iostream>

int main(int argc, char * argv[])
{
    int ret = -1;
    const char * url = "rtmp://10.11.0.196/live/zxin";
    rtmp_context_t * ctx = nullptr;
    rtmp_packet_t pkt = { 0 };

    os_rtmp_log_set_level(RTMP_LOG_LEVEL_DEBUG);

    if (0 != os_rtmp_init())
    {
        std::cout << "RTMP init failed" << std::endl;
        return EXIT_FAILURE;
    }

    ctx = os_rtmp_alloc(url, RTMP_CLIENT_READ);
    if (NULL == ctx)
    {
        std::cout << "Create rtmp instance failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (0 != os_rtmp_parse_url(ctx))
    {
        std::cout << "Parse url " << url << " failed" << std::endl;
        goto ERR;
    }
    std::cout << "Parse url successful ..." << std::endl;

    if (0 != os_rtmp_connect(ctx))
    {
        std::cout << "Connect server failed" << std::endl;
        goto ERR;
    }
    std::cout << "Connect server successful ..." << std::endl;

    if (0 != os_rtmp_handshake(ctx))
    {
        std::cout << "Handshake failed" << std::endl;
        goto ERR;
    }
    std::cout << "Handshake successful ..." << std::endl;

    if (0 != os_rtmp_connect_stream(ctx))
    {
        std::cout << "Connect stream failed" << std::endl;
        goto ERR;
    }
    std::cout << "Connect stream successful ..." << std::endl;

    //while (os_rtmp_recv_packet(ctx, &pkt))
    //{
    //    printf("Recv chunk type:%d type %x, %d bytes ...\n", pkt.rc.fmt, pkt.rc.mtid, pkt.rc.len);
    //    //std::cout << "Recv type " << pkt.type_id << " " << pkt.len << " bytes ..." << std::endl;
    //    os_rtmp_free_packet(&pkt);
    //    memset(&pkt, 0, sizeof(pkt));
    //}

    return EXIT_SUCCESS;

ERR:
    if (nullptr != ctx)
    {
        os_rtmp_free(&ctx);
        ctx = nullptr;
    }

    os_rtmp_uninit();

    return EXIT_FAILURE;
}
