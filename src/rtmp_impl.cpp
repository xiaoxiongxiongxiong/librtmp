#include "rtmp_impl.h"

#include <ws2tcpip.h>
#include <winsock.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

#include "log.h"

#define OS_RTMP_C0S0_LEN 1
#define OS_RTMP_C1S1_LEN 1536
#define OS_RTMP_C2S2_LEN 1536
#define OS_RTMP_INT24_MAX 0xFFFFFF

#define OS_RTMP_HEADER_MAX_LEN 11  // 头最大长度

static const int g_header_size[] = { 11, 7, 3, 0 };

CRtmpImpl::CRtmpImpl(const std::string & url, const RTMP_CLIENT_MODE mode) :
    _url(url),
    _mode(mode)
{
}

CRtmpImpl::~CRtmpImpl()
{
    _url.clear();
}

int CRtmpImpl::ParseUrl()
{
    if (0 != _url.compare(0, 7, "rtmp://"))
    {
        os_rtmp_log_warn("[0x%p] Unsupported stream format", this);
        return -1;
    }

    int cur_pos = 7;
    std::string cache = _url.substr(7);
    auto pos = cache.find('/');
    if (std::string::npos == pos)
    {
        os_rtmp_log_warn("[0x%p] Invalid stream format", this);
        return -1;
    }
    _host = cache.substr(0, pos);
    cache = cache.substr(pos + 1);

    pos = cache.find('/');
    if (std::string::npos == pos)
    {
        os_rtmp_log_warn("[0x%p] Invalid stream format", this);
        return -1;
    }
    _app = cache.substr(0, pos);
    cache = cache.substr(pos + 1);

    pos = cache.find('?');
    if (std::string::npos == pos)
    {
        _play_path = cache.substr(0, pos);
        _tc_url = "rtmp://" + _host + '/' + _app;

        return 0;
    }

    //ctx->play_path.len = static_cast<int>(pos);
    //ctx->play_path.data = ctx->url + cur_pos;
    //cur_pos += ctx->play_path.len;
    //ctx->tc_url.data = ctx->url;
    //ctx->tc_url.len = cur_pos;

    return 0;
}

void CRtmpImpl::SetBufferMs(int buffer_ms)
{
    _buffer_ms = buffer_ms;
}

bool CRtmpImpl::SendBytes(const char * buf, int len)
{
    bool succ = true;
    int total_bytes = 0;
    while (total_bytes < len)
    {
        if (_fd <= 0)
            break;
        const auto bytes = ::send(static_cast<SOCKET>(_fd), buf + total_bytes, len - total_bytes, 0);
        if (bytes < 0)
        {
            const int code = WSAGetLastError();
            if (-1 == bytes && WSAEWOULDBLOCK == code)
                continue;
            succ = false;
            break;
        }
        total_bytes += bytes;
    }
    return succ;
}

bool CRtmpImpl::RecvBytes(char * buf, int len)
{
    bool succ = true;
    int total_bytes = 0;

    while (total_bytes < len)
    {
        if (_fd <= 0)
            break;
        const auto bytes = ::recv(static_cast<SOCKET>(_fd), buf + total_bytes, len - total_bytes, 0);
        if (bytes <= 0)
        {
            int code = WSAGetLastError();
            if (-1 == bytes && WSAEWOULDBLOCK == code)
                continue;
            succ = false;
            break;
        }
        total_bytes += bytes;
    }

    return succ;
}

bool CRtmpImpl::SendPacket(const rtmp_packet_t & pkt, const std::string & cmd)
{
    auto rc = pkt.rc;
    int body_size = rc.len;
    int chunk_size = body_size;
    int sent_bytes = 0;

    if (!SendChunkHeader(pkt.rc))
    {
        os_rtmp_log_warn("[0x%p] Send chunk header failed", this);
        return false;
    }

    while (true)
    {
        chunk_size = min(_chunk_size, body_size - sent_bytes);

        if (!SendBytes(pkt.data + sent_bytes, chunk_size))
        {
            os_rtmp_log_error("[0x%p] Send %d bytes failed", this, chunk_size);
            return false;
        }

        sent_bytes += chunk_size;
        if (sent_bytes == body_size)
            break;

        rc.fmt = 3;
        if (!SendChunkHeader(rc))
        {
            os_rtmp_log_warn("[0x%p] Send chunk header failed", this);
            return false;
        }
    }

    if (RTMP_CTRL_PROTO_INVOKE_AMF0 == pkt.rc.mtid && !cmd.empty())
    {
        AddInvoke(cmd, _invoke_id.load());
    }

    return true;
}

bool CRtmpImpl::RecvPacket(rtmp_packet_t & pkt)
{
    int bytes = 0;
    int total_bytes = 0;
    rtmp_chunk_t cache = { 0 };

    if (!RecvChunkHeader(pkt.rc))
    {
        os_rtmp_log_warn("[0x%p] Recv chunk header failed", this);
        return false;
    }

    if (pkt.rc.len <= 0)
        return true;

    if (!os_rtmp_alloc_packet(&pkt, pkt.rc.len))
    {
        os_rtmp_log_warn("[0x%p] Alloc packet %d bytes failed", this, pkt.rc.len);
        return false;
    }

    while (true)
    {
        bytes = min(pkt.rc.len - total_bytes, _chunk_size);

        if (!RecvBytes(pkt.data + total_bytes, bytes))
        {
            os_rtmp_log_warn("[0x%p] Recv %d bytes failed", this, bytes);
            os_rtmp_free_packet(&pkt);
            return false;
        }

        total_bytes += bytes;

        if (pkt.rc.len == total_bytes)
            break;

        cache.ts_field = pkt.rc.ts_field;
        if (!RecvChunkHeader(cache))
        {
            os_rtmp_log_warn("[0x%p] Recv chunk header failed", this);
            os_rtmp_free_packet(&pkt);
            return false;
        }

        if (cache.len > 0 && pkt.rc.len != cache.len)
        {
            os_rtmp_log_warn("[0x%p] Chunk data length changed from %d to %d", this, pkt.rc.len, cache.len);
            os_rtmp_free_packet(&pkt);
            return false;
        }
        memset(&cache, 0, sizeof(cache));
    }

    return true;
}

int CRtmpImpl::ConnectServer()
{
    std::string host, port;
    u_long mode = 1;
    struct addrinfo * ais = nullptr;
    // 生成sockaddr
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    ParseHost(host, port);

    int ret = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &ais);
    if (0 != ret || nullptr == ais)
    {
        os_rtmp_log_error("[0x%p] %s", this, gai_strerror(ret));
        goto ERR;
    }

    _fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (SOCKET_ERROR == _fd)
    {
        os_rtmp_log_error("[0x%p] Create socket failed", this);
        goto ERR;
    }

    ret = ::connect(_fd, ais->ai_addr, static_cast<int>(ais->ai_addrlen));
    if (0 != ret)
    {
        os_rtmp_log_error("[0x%p] connect failed, code is %d", this, WSAGetLastError());
        goto ERR;
    }

    if (0 != ::ioctlsocket(static_cast<SOCKET>(_fd), FIONBIO, &mode))
    {
        os_rtmp_log_error("[0x%p] Set socket NOBLOCK failed", this);
        goto ERR;
    }

    ::freeaddrinfo(ais);

    return 0;
ERR:
    if (nullptr != ais)
    {
        ::freeaddrinfo(ais);
        ais = nullptr;
    }

    if (SOCKET_ERROR != _fd)
    {
        ::closesocket(static_cast<SOCKET>(_fd));
        _fd = -1;
    }

    return -1;
}

int CRtmpImpl::HandShake()
{
    char s0s1[OS_RTMP_C0S0_LEN + OS_RTMP_C1S1_LEN] = { 0 };
    char c0c1[OS_RTMP_C0S0_LEN + OS_RTMP_C1S1_LEN] = { 0x03 };
    char c2s2[OS_RTMP_C2S2_LEN] = { 0 };

    const auto ts = GetTimeStamp();
    memcpy(c0c1 + OS_RTMP_C0S0_LEN, &ts, 4);
    if (!SendBytes(c0c1, static_cast<int>(sizeof(c0c1))))
    {
        os_rtmp_log_error("[0x%p] Send c0c1 failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] Send c0c1", this);

    if (!RecvBytes(s0s1, static_cast<int>(sizeof(s0s1))))
    {
        os_rtmp_log_error("[0x%p] Recv s0s1 failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] Recv s0s1", this);

    memcpy(c2s2, s0s1 + 1, OS_RTMP_C2S2_LEN);
    if (!SendBytes(c2s2, OS_RTMP_C2S2_LEN))
    {
        os_rtmp_log_error("[0x%p] Send c2 failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] Send C2", this);

    memset(c2s2, 0, OS_RTMP_C2S2_LEN);
    if (!RecvBytes(c2s2, OS_RTMP_C2S2_LEN))
    {
        os_rtmp_log_error("[0x%p] Recv s2 failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] Recv S2, handshake over", this);

    return 0;
}

int CRtmpImpl::ConnectStream()
{
    bool res = true;
    std::string body;
    rtmp_packet_t pkt = {};
    pkt.rc.fmt = 0;
    pkt.rc.csid = 3;
    pkt.rc.mtid = RTMP_CTRL_PROTO_INVOKE_AMF0;

    amf0_encode_string_ex(body, "connect");
    amf0_encode_number(body, static_cast<double>(++_invoke_id));
    body.append(1, AMF0_DATA_TYPE_OBJECT);
    amf0_encode_avl_string_ex(body, "app", _app);
    if (!_tc_url.empty())
        amf0_encode_avl_string_ex(body, "tcUrl", _tc_url);
    if (RTMP_CLIENT_READ == _mode)
    {
        amf0_encode_avl_boolean_ex(body, "fpad", false);
        amf0_encode_avl_number_ex(body, "capabilities", 15.0);
        amf0_encode_avl_number_ex(body, "audioCodecs", RTMP_AUDIO_CODEC_AAC);
        amf0_encode_avl_number_ex(body, "videoCodecs", RTMP_VIDEO_CODEC_H264);
        amf0_encode_avl_number_ex(body, "videoFunction", 1.0);
    }
    amf0_encode_int24be(body, AMF0_DATA_TYPE_OBJECT_END);

    pkt.rc.len = static_cast<int>(body.length());
    pkt.data = const_cast<char *>(body.data());

    if (!SendPacket(pkt, "connect"))
    {
        os_rtmp_log_warn("[0x%p] Send connect packet failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] connect stream", this);

    memset(&pkt, 0, sizeof(rtmp_packet_t));
    while (res && !_playing && RecvPacket(pkt))
    {
        if (0 == pkt.rc.len && nullptr == pkt.data)
        {
            continue;
        }

        res = HandlePacket(pkt);

        os_rtmp_free_packet(&pkt);
        memset(&pkt, 0, sizeof(rtmp_packet_t));
    }

    //os_rtmp_free_packet(&pkt);
    //memset(&pkt, 0, sizeof(rtmp_packet_t));

    while (res && RecvPacket(pkt))
    {
        res = HandlePacket(pkt);
        os_rtmp_free_packet(&pkt);
        memset(&pkt, 0, sizeof(rtmp_packet_t));
    }

    return (res && _playing) ? 0 : -1;
}

int CRtmpImpl::PlayStream()
{
    std::string cache;
    rtmp_packet_t pkt = {};
    pkt.rc.csid = 0x08;
    pkt.rc.mtid = RTMP_CTRL_PROTO_INVOKE_AMF0;
    pkt.rc.msid = _stream_id;
    amf0_encode_string_ex(cache, "play");
    amf0_encode_number(cache, static_cast<double>(++_invoke_id));
    cache.append(1, AMF0_DATA_TYPE_NULL);
    amf0_encode_string_ex(cache, _play_path);
    // 0: recorded, -1: live, -2: both
    amf0_encode_number(cache, -1.0);

    pkt.data = const_cast<char *>(cache.data());
    pkt.rc.len = static_cast<int>(cache.length());

    if (!SendPacket(pkt, "play"))
    {
        os_rtmp_log_warn("[0x%p] Send play packet failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] play stream", this);

    return 0;
}

int CRtmpImpl::CreateStream()
{
    std::string cache;
    rtmp_packet_t pkt = {};
    pkt.rc.fmt = 1;
    pkt.rc.csid = 0x03;
    pkt.rc.mtid = RTMP_CTRL_PROTO_INVOKE_AMF0;
    amf0_encode_string_ex(cache, "createStream");
    amf0_encode_number(cache, static_cast<double>(++_invoke_id));
    cache.append(1, AMF0_DATA_TYPE_NULL);

    pkt.data = const_cast<char *>(cache.data());
    pkt.rc.len = static_cast<int>(cache.length());

    if (!SendPacket(pkt, "createStream"))
    {
        os_rtmp_log_warn("[0x%p] Send createStream packet failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] create stream", this);

    return 0;
}

int CRtmpImpl::PublishStream()
{
    std::string cache;
    rtmp_packet_t pkt = {};
    pkt.rc.csid = 0x03;
    pkt.rc.mtid = RTMP_CTRL_PROTO_INVOKE_AMF0;
    
    amf0_encode_string_ex(cache, "publish");
    amf0_encode_number(cache, static_cast<double>(++_invoke_id));
    cache.append(1, AMF0_DATA_TYPE_NULL);
    amf0_encode_string_ex(cache, _play_path);
    amf0_encode_string_ex(cache, _app);

    pkt.data = const_cast<char *>(cache.data());
    pkt.rc.len = static_cast<int>(cache.length());

    if (!SendPacket(pkt, "publish"))
    {
        os_rtmp_log_warn("[0x%p] Send publish packet failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] publish stream", this);

    return 0;
}

int CRtmpImpl::DeleteStream()
{
    std::string cache;
    rtmp_packet_t pkt = {};
    pkt.rc.csid = 0x03;
    pkt.rc.mtid = RTMP_CTRL_PROTO_INVOKE_AMF0;
    
    amf0_encode_string_ex(cache, "deleteStream");
    amf0_encode_number(cache, static_cast<double>(++_invoke_id));
    cache.append(1, AMF0_DATA_TYPE_NULL);
    amf0_encode_number(cache, static_cast<double>(_stream_id));

    pkt.data = const_cast<char *>(cache.data());
    pkt.rc.len = static_cast<int>(cache.length());

    if (!SendPacket(pkt))
    {
        os_rtmp_log_warn("[0x%p] Send deleteStream packet failed", this);
        return -1;
    }

    os_rtmp_log_debug("[0x%p] delete stream", this);

    return 0;
}

int CRtmpImpl::CloseStream()
{
    rtmp_packet_t pkt = {};
    std::string cache;

    return 0;
}

uint32_t CRtmpImpl::GetTimeStamp()
{
    return static_cast<uint32_t>(timeGetTime());
}

bool CRtmpImpl::ParseHost(std::string & host, std::string & port)
{
    if (_host.empty())
    {
        os_rtmp_log_warn("[0x%p] Host is empty", this);
        return false;
    }

    auto pos = _host.find_last_of(':');
    if (std::string::npos != pos)
    {
        port = _host.substr(pos + 1);
        host = _host.substr(0, pos);
    }
    else
    {
        host = _host;
        port = "1935";
    }
    return true;
}

bool CRtmpImpl::SendChunkHeader(const rtmp_chunk_t & rc)
{
    std::string header;

    if (rc.fmt > 3 || rc.fmt < 0)
    {
        os_rtmp_log_warn("[0x%p] Invalid header type: %d", this, rc.fmt);
        return false;
    }

    // fmt
    header.append(1, 0x00);
    header[0] = rc.fmt << 6;

    // cs id
    if (rc.csid < 64)
        header[0] |= rc.csid;
    else if (rc.csid < 320)
    {
        header[0] &= 0xc0;
        header.append(1, (rc.csid - 64) & 0xff);
    }
    else
    {
        header[0] |= 0x3f;
        header.append(1, (rc.csid - 64) & 0xff);
        header.append(1, (rc.csid - 64) >> 8);
    }

    // message header
    const auto timestamp = rc.timestamp;
    if (rc.fmt < 3)
    {
        const auto val = min(OS_RTMP_INT24_MAX, timestamp);
        amf0_encode_int24be(header, val);
    }
    if (rc.fmt < 2)
    {
        amf0_encode_int24be(header, rc.len);
        header.append(1, rc.mtid);
    }
    if (0 == rc.fmt)
        amf0_encode_int32le(header, rc.msid);

    // Extended Timestamp
    if (timestamp > OS_RTMP_INT24_MAX)
        amf0_encode_int32be(header, timestamp);

    return SendBytes(header.data(), static_cast<int>(header.length()));
}

bool CRtmpImpl::RecvChunkHeader(rtmp_chunk_t & rc)
{
    uint8_t cbh = 0;
    uint8_t csid = 0;
    int header_size = 0;
    char header[OS_RTMP_HEADER_MAX_LEN] = { 0 };

    if (!RecvBytes(reinterpret_cast<char *>(&cbh), 1))
    {
        os_rtmp_log_warn("[0x%p] Recv 1 bytes failed", this);
        return false;
    }

    rc.fmt = (cbh & 0xc0) >> 6;
    csid = cbh & 0x3f;
    if (csid < 2)
    {
        if (!RecvBytes(reinterpret_cast<char *>(&csid), 1))
        {
            os_rtmp_log_warn("[0x%p] Recv 1 bytes failed", this);
            return false;
        }
        rc.csid = static_cast<int>(csid) + 64;
    }
    else if (csid > 63)
    {
        uint8_t buff[2] = { 0 };
        if (!RecvBytes(reinterpret_cast<char *>(buff), 2))
        {
            os_rtmp_log_warn("[0x%p] Recv 2 bytes failed", this);
            return false;
        }
        rc.csid = buff[1] * 256 + buff[0] + 64;
    }
    else
        rc.csid = static_cast<int>(csid);

    header_size = g_header_size[rc.fmt];
    if (header_size < 3)
        goto end;

    if (!RecvBytes(header, header_size))
    {
        os_rtmp_log_warn("[0x%p] Recv %d bytes failed", this, header_size);
        return false;
    }

    // timestamp
    rc.ts_field = amf0_decode_int24(header);

    if (header_size > 6)
    {
        rc.len = amf0_decode_int24(header + 3);
        rc.mtid = header[6];
    }
    if (header_size > 10)
        rc.msid = amf0_decode_int32le(header + 7);

    if (header_size > 6 && (rc.mtid > RTMP_CTRL_PROTO_METADATA || RTMP_CTRL_PROTO_SET_CHUNK_SIZE > rc.mtid))
    {
        os_rtmp_log_warn("[0x%p] Invalid message type id: %d", this, rc.mtid);
        return false;
    }

end:
    if (OS_RTMP_INT24_MAX == rc.ts_field)
    {
        memset(header, 0, sizeof(header));
        if (!RecvBytes(header, 4))
        {
            os_rtmp_log_warn("[0x%p] Recv 'Extended Timestamp' failed", this);
            return false;
        }
        rc.timestamp = amf0_decode_int32le(header);
    }
    else
        rc.timestamp = rc.ts_field;

    //os_rtmp_log_debug("[0x%p] fmt: %d, csid: %d, mtid: %d, header size: %d, len: %d",
    //                  this, rc.fmt, rc.csid, rc.mtid, header_size, rc.len);

    return true;
}

bool CRtmpImpl::HandlePacket(const rtmp_packet_t & pkt)
{
    bool ret = true;
    switch (pkt.rc.mtid)
    {
    case RTMP_CTRL_PROTO_SET_CHUNK_SIZE:
        OnSetChunkSize(pkt.data, pkt.rc.len);
        break;
    case RTMP_CTRL_PROTO_USER_CTRL_MSG:
        OnUserControlMessage(pkt.data, pkt.rc.len);
        break;
    case RTMP_CTRL_PROTO_WINDOW_ACK_SIZE:
        OnWindowAcknowledgementSize(pkt.data, pkt.rc.len);
        break;
    case RTMP_CTRL_PROTO_SET_PEER_BANDWIDTH:
        OnSetPeerBandwidth(pkt.data, pkt.rc.len);
        break;
    case RTMP_CTRL_PROTO_AUDIO_MSG:
        os_rtmp_log_debug("[0x%p] Recv %d bytes audio msg ...", this, pkt.rc.len);
        _has_audio = true;
        break;
    case RTMP_CTRL_PROTO_VIDEO_MSG:
        os_rtmp_log_debug("[0x%p] Recv %d bytes video msg ...", this, pkt.rc.len);
        _has_video = true;
        break;
    case RTMP_CTRL_PROTO_DATA_AMF0:
        ret = OnDataMessage(pkt.data, pkt.rc.len);
        break;
    case RTMP_CTRL_PROTO_INVOKE_AMF0:
        ret = OnInvokePacketAmf0(pkt.data, pkt.rc.len);
        break;
    default:
        os_rtmp_log_info("[0x%p] Unsupported type: %d ...", this, pkt.rc.mtid);
        break;
    }

    return ret;
}

void CRtmpImpl::OnSetChunkSize(const char * data, const int len)
{
    if (nullptr == data || 4 != len)
    {
        os_rtmp_log_warn("[0x%p] Invalid chunk size packet", this);
        return;
    }
    _chunk_size = amf0_decode_int32be(data);
    os_rtmp_log_info("[0x%p] chunk size is %d", this, _chunk_size);
}

void CRtmpImpl::OnUserControlMessage(const char * data, const int len)
{
    if (nullptr == data || len <= 0)
    {
        os_rtmp_log_warn("[0x%p] Invalid user control message", this);
        return;
    }

    const auto ev_type = static_cast<RTMP_USER_CTRL_EVENT_TYPE>(amf0_decode_int16(data));
    switch (ev_type)
    {
    case RTMP_CTRL_EVENT_STREAM_BEGIN:
        os_rtmp_log_info("[0x%p] Recv 'Stream Begin' from server", this);
        break;
    case RTMP_CTRL_EVENT_STREAM_EOF:
        os_rtmp_log_info("[0x%p] Recv 'Stream EOF' from server", this);
        break;
    case RTMP_CTRL_EVENT_STREAM_DRY:
        os_rtmp_log_info("[0x%p] Recv 'StreamDry' from server", this);
        break;
    case RTMP_CTRL_EVENT_SET_BUFF_LEN:
        os_rtmp_log_info("[0x%p] Recv 'SetBuffer Length' from client", this);
        break;
    case RTMP_CTRL_EVENT_STREAM_IS_RECORD:
        os_rtmp_log_info("[0x%p] Recv 'StreamIsRecorded' from server", this);
        break;
    case RTMP_CTRL_EVENT_PING_REQUEST:
        _pts.store(amf0_decode_int32be(data + 2));
        os_rtmp_log_debug("[0x%p] Recv 'PingRequest' from server, timestamp is %u", this, _pts.load());
        SendUserCtrlEvent(RTMP_CTRL_EVENT_PING_RESPONSE, _stream_id, 0);
        break;
    case RTMP_CTRL_EVENT_PING_RESPONSE:
        os_rtmp_log_info("[0x%p] Recv 'PingResponse' from client", this);
        break;
    default:
        break;
    }
}

void CRtmpImpl::OnWindowAcknowledgementSize(const char * data, const int len)
{
    if (nullptr == data || 4 != len)
    {
        os_rtmp_log_warn("[0x%p] Invalid Window Acknowledgement Size packet", this);
        return;
    }

    _wnd_ack_size = amf0_decode_int32be(data);
    os_rtmp_log_info("[0x%p] window acknowledgement size is %d", this, _wnd_ack_size);
}

void CRtmpImpl::OnSetPeerBandwidth(const char * data, const int len)
{
    if (nullptr == data || 5 != len)
    {
        os_rtmp_log_warn("[0x%p] Invalid Peer Bandwidth", this);
        return;
    }

    _peer_bandwidth = amf0_decode_int32be(data);
    os_rtmp_log_info("[0x%p] peer bandwidth is %d", this, _peer_bandwidth);
}

bool CRtmpImpl::OnDataMessage(const char * data, int len)
{
    amf0_object_t obj = {}, tmp = {};
    amf0_avl_t method = {};
    amf0_property_t * prop = nullptr;

    if (nullptr == data || len <= 0)
    {
        os_rtmp_log_warn("[0x%p] Invalid data message", this);
        return false;
    }

    if (len != amf0_decode_object(data, len, false, obj))
    {
        os_rtmp_log_warn("[0x%p] Decode data message failed", this);
        return false;
    }

    prop = amf0_get_property(obj, nullptr, 0);
    amf0_get_string(*prop, method);

    if (amf0_match(method, amf0_avl("onMetaData")))
    {
        prop = amf0_get_property(obj, nullptr, 1);
        if (nullptr == prop || (AMF0_DATA_TYPE_ECMA_ARRAY != prop->type && AMF0_DATA_TYPE_OBJECT != prop->type))
            goto end;

        _recv_metadata = true;
        tmp = prop->vu.obj;
        for (int i = 0; i < tmp.cnt; ++i)
        {
            prop = amf0_get_property(tmp, nullptr, i);
            if (amf0_match(prop->name, amf0_avl("videocodecid")))
                _has_video = true;
            else if (amf0_match(prop->name, amf0_avl("audiocodecid")))
                _has_audio = true;
        }
    }
    else if (amf0_match(method, amf0_avl("@setDataFrame")))
    {
        os_rtmp_log_debug("[0x%p] Recv '@setDataFrame', skip it");
    }

end:
    amf0_object_reset(obj);

    return true;
}

bool CRtmpImpl::OnInvokePacketAmf0(const char * data, const int len)
{
    if (nullptr == data || len <= 0)
    {
        os_rtmp_log_warn("[0x%p] Invalid invoke packet", this);
        return false;
    }

    amf0_object_t obj = {};
    if (len != amf0_decode_object(data, len, false, obj))
    {
        os_rtmp_log_warn("[0x%p] Decode object failed", this);
        return false;
    }

    amf0_avl_t method = {};
    auto * prop = amf0_get_property(obj, nullptr, 0);
    amf0_get_string(*prop, method);

    bool ret = true;
    auto tid = static_cast<int>(amf0_get_property(obj, nullptr, 1)->vu.num);
    if (amf0_match(method, amf0_avl("_result")))
    {
        ret = OnInvokeResult(obj, tid);
    }
    else if (amf0_match(prop->vu.vl, amf0_avl(" _error")))
    {

    }
    else if (amf0_match(method, amf0_avl("onStatus")))
    {
        ret = OnInvokeStatus(obj, tid);
    }
    amf0_object_reset(obj);

    return ret;
}

bool CRtmpImpl::OnInvokeError(const amf0_object_t & obj, int id)
{
    return true;
}

bool CRtmpImpl::OnInvokeResult(const amf0_object_t & obj, int id)
{
    std::string cmd;
    if (!DelInvoke("", id, cmd))
    {
        return false;
    }
    if (cmd == "connect")
    {
        if (!SetWindowAcknowledgementSize())
        {
            os_rtmp_log_warn("[0x%p] Set window acknowledgement size failed", this);
            return false;
        }
        if (!SendUserCtrlEvent(RTMP_CTRL_EVENT_SET_BUFF_LEN, 0, 300))
        {
            os_rtmp_log_warn("[0x%p] Send user control event failed", this);
            return false;
        }
        if (0 != CreateStream())
        {
            os_rtmp_log_warn("[0x%p] Create stream failed", this);
            return false;
        }
    }
    else if (cmd == "createStream")
    {
        _stream_id = static_cast<int>(amf0_get_property(obj, nullptr, 3)->vu.num);
        os_rtmp_log_info("[0x%p] stream id is %d", this, _stream_id);
        if (RTMP_CLIENT_WRITE == _mode)
        {
            PublishStream();
            return true;
        }

        if (0 != PlayStream())
        {
            os_rtmp_log_warn("[0x%p] Play stream failed", this);
            return false;
        }
        if (!SendUserCtrlEvent(RTMP_CTRL_EVENT_SET_BUFF_LEN, _stream_id, 3000))
        {
            os_rtmp_log_warn("[0x%p] Send user control event failed", this);
            return false;
        }
    }

    return true;
}

bool CRtmpImpl::OnInvokeStatus(const amf0_object_t & obj, int id)
{
    auto * prop = amf0_get_property(obj, nullptr, 3);
    if (nullptr == prop || AMF0_DATA_TYPE_OBJECT != prop->type)
    {
        os_rtmp_log_warn("[0x%p] Get onStatus result failed", this);
        return false;
    }

    const auto & tmp = prop->vu.obj;
    const auto * level_prop = amf0_get_property(tmp, "level", -1);
    if (nullptr == level_prop)
    {
        os_rtmp_log_warn("[0x%p] Get level property failed", this);
        return false;
    }

    const auto * level_str = amf0_get_string_ex(*level_prop);
    if (nullptr == level_str || 0 != strcmp(level_str, "status"))
    {
        const char * desc_str = nullptr;
        const auto * desc_prop = amf0_get_property(tmp, "description", -1);
        if (nullptr != desc_prop && nullptr != (desc_str = amf0_get_string_ex(*desc_prop)))
        {
            os_rtmp_log_warn("[0x%p] Server error: %s", this, desc_str);
        }

        return false;
    }

    const char * code_str = nullptr;
    const auto * code_prop = amf0_get_property(tmp, "code", -1);
    if (nullptr != code_prop && nullptr != (code_str = amf0_get_string_ex(*code_prop)))
    {
        if (!strcmp(code_str, "NetStream.Play.Start"))
            _playing = true;
        else if (!strcmp(code_str, "NetStream.Play.Stop"))
            _playing = false;
        else if (!strcmp(code_str, "NetStream.Play.UnpublishNotify"))
            _playing = false;
        else if (!strcmp(code_str, "NetStream.Publish.Start"))
            _playing = false;
        else if (!strcmp(code_str, "NetStream.Seek.Notify"))
            _playing = true;
        else if (!strcmp(code_str, "NetStream.Data.Start"))
            _playing = true;
    }

    return true;
}

bool CRtmpImpl::SetWindowAcknowledgementSize()
{
    std::string cache;
    rtmp_packet_t pkt = { 0 };
    pkt.rc.csid = 0x02;
    pkt.rc.mtid = RTMP_CTRL_PROTO_WINDOW_ACK_SIZE;
    amf0_encode_int32be(cache, _wnd_ack_size);
    pkt.rc.len = static_cast<int>(cache.length());
    pkt.data = const_cast<char *>(cache.c_str());

    return SendPacket(pkt);
}

bool CRtmpImpl::SendUserCtrlEvent(const RTMP_USER_CTRL_EVENT_TYPE type, int stream_id, const int buffer_ms)
{
    std::string cache;
    rtmp_packet_t pkt = {};

    pkt.rc.fmt = 1;
    pkt.rc.csid = 0x02;
    pkt.rc.mtid = RTMP_CTRL_PROTO_USER_CTRL_MSG;

    amf0_encode_int16(cache, static_cast<int16_t>(type));

    switch (type)
    {
    case RTMP_CTRL_EVENT_STREAM_BEGIN:
        amf0_encode_int32be(cache, stream_id);
        break;
    case RTMP_CTRL_EVENT_STREAM_EOF:
        amf0_encode_int32be(cache, stream_id);
        break;
    case RTMP_CTRL_EVENT_STREAM_DRY:
        amf0_encode_int32be(cache, stream_id);
        break;
    case RTMP_CTRL_EVENT_SET_BUFF_LEN:
        amf0_encode_int32be(cache, stream_id);
        amf0_encode_int32be(cache, buffer_ms);
        break;
    case RTMP_CTRL_EVENT_STREAM_IS_RECORD:
        amf0_encode_int32be(cache, stream_id);
        break;
    case RTMP_CTRL_EVENT_PING_REQUEST:
        amf0_encode_int32be(cache, GetTimeStamp());
        break;
    case RTMP_CTRL_EVENT_PING_RESPONSE:
        amf0_encode_int32be(cache, _pts.load());
        break;
    default:
        break;
    }

    os_rtmp_log_debug("[0x%p] Send user ctrl event, type is %d", this, type);

    pkt.data = const_cast<char *>(cache.c_str());
    pkt.rc.len = static_cast<int>(cache.length());

    return SendPacket(pkt);
}

bool CRtmpImpl::AddInvoke(const std::string & cmd, int id)
{
    if (id < 0 || cmd.empty())
    {
        os_rtmp_log_warn("[0x%p] Invalid method id: %d or data", this, id);
        return false;
    }

    std::unique_lock<std::mutex> lck(_mtx);
    _invokes.emplace(id, cmd);
    return true;
}

bool CRtmpImpl::DelInvoke(const std::string & cmd, const int id, std::string & res)
{
    std::unique_lock<std::mutex> lck(_mtx);
    auto found = _invokes.find(id);
    if (_invokes.end() == found)
    {
        found = std::find_if(_invokes.begin(), _invokes.end(),
                             [&cmd](const std::pair<int, std::string> & inv)
        {
            return cmd == inv.second;
        });
    }
    if (_invokes.end() != found)
    {
        res = (*found).second;
        _invokes.erase(found);
        return true;
    }
    return false;
}

