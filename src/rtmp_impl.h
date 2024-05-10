#include <cstdint>
#include <cstdbool>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "rtmp.h"

class CRtmpImpl
{
public:
    CRtmpImpl(const std::string & url, RTMP_CLIENT_MODE mode);
    ~CRtmpImpl();

    // 解析url
    int ParseUrl();

    //
    void SetBufferMs(int buffer_ms);

    // 发送
    bool SendBytes(const char * buf, int len);

    // 接收
    bool RecvBytes(char * buf, int len);

    // 发送数据包
    bool SendPacket(const rtmp_packet_t & pkt, const std::string & cmd = "");

    // 接收数据包
    bool RecvPacket(rtmp_packet_t & pkt);

    // 连接
    int ConnectServer();

    // 握手
    int HandShake();

    // 连接app实例
    int ConnectStream();

    // 
    int PlayStream();

    // 
    int CreateStream();

    //
    int PublishStream();

    //
    int DeleteStream();

    //
    int CloseStream();

protected:
    // 获取时间戳
    uint32_t GetTimeStamp();

    // 解析host
    bool ParseHost(std::string & host, std::string & port);

    // 发送chunk头
    bool SendChunkHeader(const rtmp_chunk_t & rc);

    //
    bool RecvChunkHeader(rtmp_chunk_t & rc);

    // 处理数据包
    bool HandlePacket(const rtmp_packet_t & pkt);

    // 设置chunk size
    void OnSetChunkSize(const char * data, int len);

    // User Control Message
    void OnUserControlMessage(const char * data, int len);

    // 收到Window Acknowledgement Size
    void OnWindowAcknowledgementSize(const char * data, int len);

    // 收到Set Peer Bandwidth
    void OnSetPeerBandwidth(const char * data, int len);

    // 
    bool OnDataMessage(const char * data, int len);

    // 
    bool OnInvokePacketAmf0(const char * data, int len);

    //
    bool OnInvokeError(const amf0_object_t & obj, int id);

    //
    bool OnInvokeResult(const amf0_object_t & obj, int id);

    //
    bool OnInvokeStatus(const amf0_object_t & obj, int id);

    // Window Acknowledgement Size
    bool SetWindowAcknowledgementSize();

    // 发送用于操作事件
    bool SendUserCtrlEvent(RTMP_USER_CTRL_EVENT_TYPE type, int stream_id, int buffer_ms);

    // 添加调用方法
    bool AddInvoke(const std::string & cmd, int id);
    // 删除调用方法
    bool DelInvoke(const std::string & cmd, int id, std::string & res);

private:
    // url
    std::string _url;

    // 模式
    RTMP_CLIENT_MODE _mode = RTMP_CLIENT_NONE;

    // socket 句柄
    int _fd = -1;

    // 服务host
    std::string _host;

    // app
    std::string _app;
    // 播放路径
    std::string _play_path;
    // tc url
    std::string _tc_url;

    // stream id
    int _stream_id = -1;
    // 播放中
    std::atomic_bool _playing = { false };

    bool _has_video = false;
    bool _has_audio = false;
    bool _recv_metadata = false;

    // chunk size
    int _chunk_size = 128;
    // Window Acknowledgement Size
    int _wnd_ack_size = 0;
    // Peer Bandwidth
    int _peer_bandwidth = 0;
    //
    int _buffer_ms = 30000;

    // ping时间戳
    std::atomic_uint32_t _pts = { 0 };

    // 交互id
    std::atomic_int _invoke_id = { 0 };
    //
    std::mutex _mtx;
    // 操作
    std::unordered_map<int, std::string> _invokes;

    // 历史操作包
    std::unordered_map<int, rtmp_packet_t> _pkts;
};