#include "rtmp.h"
#include <stdlib.h>
#include <string.h>
#include <string>
#include <winsock.h>
#pragma comment(lib, "ws2_32.lib")

#include "log.h"
#include "rtmp_impl.h"

struct _rtmp_context_t
{
    CRtmpImpl * impl;
};

static bool os_rtmp_ctx_valid(const rtmp_context_t * ctx);

int os_rtmp_init()
{
    WSADATA wsa = {};
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (0 != res)
    {
        os_rtmp_log_error("WSAStartup failed");
        return -1;
    }

    if (2 != LOBYTE(wsa.wVersion) || 2 != HIBYTE(wsa.wVersion))
    {
        os_rtmp_log_error("Invalid socket version");
        WSACleanup();
        return -1;
    }

    return 0;
}

void os_rtmp_uninit()
{
    WSACleanup();
}

rtmp_context_t * os_rtmp_alloc(const char * url, RTMP_CLIENT_MODE mode)
{
    rtmp_context_t * ctx = nullptr;

    if (nullptr == url)
    {
        os_rtmp_log_warn("Url pointer is nullptr");
        return nullptr;
    }

    if (0 != strncmp(url, "rtmp://", 7))
    {
        os_rtmp_log_warn("Unsupported url, should be start with 'rtmp://'");
        return nullptr;
    }

    if (mode <= RTMP_CLIENT_NONE || mode >= RTMP_CLIENT_MAX)
    {
        os_rtmp_log_warn("Unsupported client mode %d", mode);
        return nullptr;
    }

    ctx = static_cast<rtmp_context_t *>(calloc(1, sizeof(rtmp_context_t)));
    if (nullptr == ctx)
    {
        os_rtmp_log_error("No enough memory");
        return nullptr;
    }

    ctx->impl = new(std::nothrow) CRtmpImpl(url, mode);
    if (nullptr == ctx->impl)
    {
        os_rtmp_log_error("No enough memory");
        free(ctx);
        return nullptr;
    }

    return ctx;
}

void os_rtmp_free(rtmp_context_t ** ctx)
{
    if (nullptr == ctx || nullptr == *ctx)
        return;

    if (nullptr != (*ctx)->impl)
    {
        delete (*ctx)->impl;
        (*ctx)->impl = nullptr;
    }

    free(*ctx);
    *ctx = nullptr;
}

int os_rtmp_parse_url(const rtmp_context_t * ctx)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->ParseUrl();
    }
    return false;
}

int os_rtmp_connect(const rtmp_context_t * ctx)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->ConnectServer();
    }
    return false;
}

bool os_rtmp_send_bytes(const rtmp_context_t * ctx, const char * buf, const int len)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->SendBytes(buf, len);
    }
    return false;
}

bool os_rtmp_recv_bytes(const rtmp_context_t * ctx, char * buf, const int len)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->RecvBytes(buf, len);
    }
    return false;
}

bool os_rtmp_alloc_packet(rtmp_packet_t * pkt, int len)
{
    if (nullptr == pkt || nullptr != pkt->data || len <= 0)
    {
        os_rtmp_log_warn("Invalid params");
        return false;
    }

    pkt->data = static_cast<char *>(calloc(static_cast<size_t>(len), 1));
    if (nullptr == pkt->data)
    {
        os_rtmp_log_error("No enough memory");
        return false;
    }
    pkt->rc.len = len;

    return true;
}

void os_rtmp_free_packet(rtmp_packet_t * pkt)
{
    if (nullptr == pkt || nullptr == pkt->data)
        return;
    free(pkt->data);
    pkt->data = nullptr;
    pkt->rc.len = 0;
}

bool os_rtmp_send_packet(const rtmp_context_t * ctx, const rtmp_packet_t * pkt)
{
    if (os_rtmp_ctx_valid(ctx) && nullptr != pkt)
    {
        return ctx->impl->SendPacket(*pkt);
    }
    return false;
}

bool os_rtmp_recv_packet(const rtmp_context_t * ctx, rtmp_packet_t * pkt)
{
    if (os_rtmp_ctx_valid(ctx) && nullptr != pkt)
    {
        return ctx->impl->RecvPacket(*pkt);
    }
    return false;
}

int os_rtmp_handshake(const rtmp_context_t * ctx)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->HandShake();
    }
    return false;
}

int os_rtmp_connect_stream(const rtmp_context_t * ctx)
{
    if (os_rtmp_ctx_valid(ctx))
    {
        return ctx->impl->ConnectStream();
    }
    return -1;
}

bool os_rtmp_ctx_valid(const rtmp_context_t * ctx)
{
    if (nullptr == ctx || nullptr == ctx->impl)
    {
        os_rtmp_log_warn("Input param is nullptr");
        return false;
    }
    return true;
}
