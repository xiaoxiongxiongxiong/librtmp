#ifndef _OS_RTMP_H_
#define _OS_RTMP_H_

#include "amf0.h"

#ifdef __cplusplus
#define OS_RTMP_API_BEGIN extern "C" {
#define OS_RTMP_API_END }
#else
#define OS_RTMP_API_BEGIN
#define OS_RTMP_API_END
#endif

#if defined(WIN32) || defined(_WIN32)
#if defined(OS_RTMP_API_EXPORT)
#define OS_RTMP_API __declspec(dllexport)
#else
#define OS_RTMP_API __declspec(dllimport)
#endif
#else
#define OS_RTMP_API
#endif

typedef enum _RTMP_CLIENT_MODE
{
    RTMP_CLIENT_NONE,
    RTMP_CLIENT_WRITE,
    RTMP_CLIENT_READ,
    RTMP_CLIENT_MAX
} RTMP_CLIENT_MODE;

// 1~6 chunk stream id should be 2

typedef enum _RTMP_CTRL_PROTO_TYPE
{
    RTMP_CTRL_PROTO_SET_CHUNK_SIZE = 0x01,
    RTMP_CTRL_PROTO_ABORT_MSG = 0x02,
    RTMP_CTRL_PROTO_ACKNOWLEDGEMENT = 0x03,
    RTMP_CTRL_PROTO_USER_CTRL_MSG = 0x04,
    RTMP_CTRL_PROTO_WINDOW_ACK_SIZE = 0x05,
    RTMP_CTRL_PROTO_SET_PEER_BANDWIDTH = 0x06,
    RTMP_CTRL_PROTO_AUDIO_MSG = 0x08,
    RTMP_CTRL_PROTO_VIDEO_MSG = 0x09,
    RTMP_CTRL_PROTO_DATA_AMF3 = 0x0f,
    RTMP_CTRL_PROTO_SHARED_OBJ_AMF3 = 0x10,
    RTMP_CTRL_PROTO_INVOKE_AMF3 = 0x11,
    RTMP_CTRL_PROTO_DATA_AMF0 = 0x12,
    RTMP_CTRL_PROTO_SHARED_OBJ_AMF0 = 0x13,
    RTMP_CTRL_PROTO_INVOKE_AMF0 = 0x14,
    RTMP_CTRL_PROTO_METADATA = 0x16,
} RTMP_CTRL_PROTO_TYPE;

typedef enum _RTMP_USER_CTRL_EVENT_TYPE
{
    RTMP_CTRL_EVENT_STREAM_BEGIN = 0x00,
    RTMP_CTRL_EVENT_STREAM_EOF,
    RTMP_CTRL_EVENT_STREAM_DRY,
    RTMP_CTRL_EVENT_SET_BUFF_LEN,
    RTMP_CTRL_EVENT_STREAM_IS_RECORD,
    RTMP_CTRL_EVENT_PING_REQUEST = 0x06,
    RTMP_CTRL_EVENT_PING_RESPONSE,
} RTMP_USER_CTRL_EVENT_TYPE;

typedef enum _RTMP_AUDIO_CODEC_TYPE
{
    RTMP_AUDIO_CODEC_NONE = 0x0001,
    RTMP_AUDIO_CODEC_ADPCM = 0x0002,
    RTMP_AUDIO_CODEC_MP3 = 0x0004,
    RTMP_AUDIO_CODEC_INTEL = 0x0008,   // not used
    RTMP_AUDIO_CODEC_UNUSED = 0x0010,  // not used
    RTMP_AUDIO_CODEC_NELLY8 = 0x0020,
    RTMP_AUDIO_CODEC_NELLY = 0x0040,
    RTMP_AUDIO_CODEC_G711A = 0x0080,   // Flash Media Server only
    RTMP_AUDIO_CODEC_G711U = 0x0100,   // Flash Media Server only
    RTMP_AUDIO_CODEC_NELLY16 = 0x0200, 
    RTMP_AUDIO_CODEC_AAC = 0x0400,
    RTMP_AUDIO_CODEC_SPEEX = 0x0800,
    RTMP_AUDIO_CODEC_ALL = 0x0FFF,
} RTMP_AUDIO_CODEC_TYPE;

typedef enum _RTMP_VIDEO_CODEC_TYPE
{
    RTMP_VIDEO_CODEC_UNUSED = 0x0001,
    RTMP_VIDEO_CODEC_JPEG = 0x0002,
    RTMP_VIDEO_CODEC_SORENSON = 0x0004,
    RTMP_VIDEO_CODEC_HOMEBREW = 0x0008,
    RTMP_VIDEO_CODEC_VP6 = 0x0010,
    RTMP_VIDEO_CODEC_VP6ALPHA = 0x0020,
    RTMP_VIDEO_CODEC_HOMEBREWV = 0x0040,
    RTMP_VIDEO_CODEC_H264 = 0x0080,
    RTMP_VIDEO_CODEC_ALL = 0x00FF,
} RTMP_VIDEO_CODEC_TYPE;

typedef struct _rtmp_context_t rtmp_context_t;

typedef struct _rtmp_chunk_t
{
    int fmt;           // 类型 0-3
    int csid;          // chunk stream id
    int msid;          // message stream id
    int len;           // 数据长度
    uint32_t timestamp;// packet full timestamp
    uint32_t ts_field; // 24-bit timestamp
    uint8_t mtid;      // message type id
} rtmp_chunk_t;

typedef struct _rtmp_packet_t 
{
    char * data;     // chunk data
    rtmp_chunk_t rc; // chunk header
} rtmp_packet_t;

OS_RTMP_API_BEGIN

/**
 *
 */
OS_RTMP_API int os_rtmp_init();

/**
 * 
 */
OS_RTMP_API void os_rtmp_uninit();

/**
 *
 */
OS_RTMP_API rtmp_context_t * os_rtmp_alloc(const char * url, RTMP_CLIENT_MODE mode);

/**
 * 
 */
OS_RTMP_API void os_rtmp_free(rtmp_context_t ** ctx);

/**
 * 
 */
OS_RTMP_API int os_rtmp_parse_url(const rtmp_context_t * ctx);

/**
 * 
 */
OS_RTMP_API int os_rtmp_connect(const rtmp_context_t * ctx);

/**
 * 
 */
OS_RTMP_API bool os_rtmp_send_bytes(const rtmp_context_t * ctx, const char * buf, int len);

/**
 * 
 */
OS_RTMP_API bool os_rtmp_recv_bytes(const rtmp_context_t * ctx, char * buf, int len);

/**
 * 
 */
OS_RTMP_API bool os_rtmp_alloc_packet(rtmp_packet_t * pkt, int len);

/**
 * 
 */
OS_RTMP_API void os_rtmp_free_packet(rtmp_packet_t * pkt);

/**
 * 
 */
OS_RTMP_API bool os_rtmp_send_packet(const rtmp_context_t * ctx, const rtmp_packet_t * pkt);

/**
 * 
 */
OS_RTMP_API bool os_rtmp_recv_packet(const rtmp_context_t * ctx, rtmp_packet_t * pkt);

/**
 * 
 */
OS_RTMP_API int os_rtmp_handshake(const rtmp_context_t * ctx);

/**
 * 
 */
OS_RTMP_API int os_rtmp_connect_stream(const rtmp_context_t * ctx);

OS_RTMP_API_END

#endif
