#include "amf0.h"

#include <vector>

#include "log.h"

#define AMF0_DOUBLE_ZERO (1e-15)

static bool amf0_add_props(const std::vector<amf0_property_t> & props, amf0_object_t & obj);

void amf0_encode_int16(std::string & buf, int16_t num)
{
    buf.append(1, num >> 8);
    buf.append(1, num & 0xff);
}

uint16_t amf0_decode_int16(const char * buf)
{
    uint16_t val = 0;
    uint8_t * data = reinterpret_cast<uint8_t *>(const_cast<char *>(buf));
    val = (data[0] << 8) | data[1];
    return val;
}

void amf0_encode_int24be(std::string & buf, int32_t num)
{
    buf.append(1, num >> 16);
    buf.append(1, num >> 8);
    buf.append(1, num & 0xff);
}

void amf0_encode_int24le(std::string & buf, int32_t num)
{
    buf.append(1, num & 0xff);
    buf.append(1, num >> 8);
    buf.append(1, num >> 16);
}

uint32_t amf0_decode_int24(const char * buf)
{
    uint32_t val = 0;
    uint8_t * data = reinterpret_cast<uint8_t *>(const_cast<char *>(buf));
    val = (data[0] << 16) | (data[1] << 8) | data[2];
    return val;
}

void amf0_encode_int32be(std::string & buf, int32_t num)
{
    buf.append(1, num >> 24);
    buf.append(1, num >> 16);
    buf.append(1, num >> 8);
    buf.append(1, num & 0xff);
}

uint32_t amf0_decode_int32be(const char * buf)
{
    uint32_t val = 0;
    uint8_t * data = reinterpret_cast<uint8_t *>(const_cast<char *>(buf));
    val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    return val;
}

void amf0_encode_int32le(std::string & buf, int32_t num)
{
    buf.append(1, num & 0xff);
    buf.append(1, num >> 8);
    buf.append(1, num >> 16);
    buf.append(1, num >> 24);
}

uint32_t amf0_decode_int32le(const char * buf)
{
    uint8_t * data = reinterpret_cast<uint8_t *>(const_cast<char*>(buf));
    uint32_t val = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
    return val;
}

void amf0_encode_number(std::string & buf, double num)
{
    uint8_t * tmp = reinterpret_cast<uint8_t *>(&num);
    buf.append(1, AMF0_DATA_TYPE_NUMBER);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (int i = 0; i < 8; ++i)
    {
        buf.append(1, tmp[7 - i]);
    }
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (int i = 0; i < 8; ++i)
    {
        buf.append(1, tmp[i]);
    }
#endif
}

double amf0_decode_number(const char * buf)
{
    double val;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t * vi = reinterpret_cast<uint8_t *>(const_cast<char *>(buf));
    uint8_t * vo = reinterpret_cast<uint8_t *>(&val);
    vo[0] = vi[7];
    vo[1] = vi[6];
    vo[2] = vi[5];
    vo[3] = vi[4];
    vo[4] = vi[3];
    vo[5] = vi[2];
    vo[6] = vi[1];
    vo[7] = vi[0];
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    memcpy(&val, buf, 8);
#endif
    return val;
}

bool amf0_encode_avl_number(std::string & buf, const amf0_avl_t & name, double num)
{
    if (nullptr == name.data || name.len <= 0)
    {
        os_rtmp_log_warn("No buffer pointer/empty buffer");
        return false;
    }

    amf0_encode_int16(buf, name.len);
    buf.append(name.data, static_cast<size_t>(name.len));
    amf0_encode_number(buf, num);

    return true;
}

bool amf0_encode_avl_number_ex(std::string & buf, const std::string & name, double num)
{
    amf0_avl_t avl = {};
    avl.len = static_cast<int>(name.length());
    avl.data = const_cast<char *>(name.data());
    return amf0_encode_avl_number(buf, avl, num);
}

void amf0_encode_boolean(std::string & buf, const bool num)
{
    buf.append(1, AMF0_DATA_TYPE_BOOLEAN);
    buf.append(1, num ? 0x01 : 0x00);
}

bool amf0_decode_boolean(const char * buf)
{
    return 0 != *buf;
}

bool amf0_encode_avl_boolean(std::string & buf, const amf0_avl_t & name, bool num)
{
    if (nullptr == name.data || name.len <= 0)
    {
        os_rtmp_log_warn("No buffer pointer/empty buffer");
        return false;
    }

    amf0_encode_int16(buf, name.len);
    buf.append(name.data, static_cast<size_t>(name.len));
    amf0_encode_boolean(buf, num);

    return true;
}

bool amf0_encode_avl_boolean_ex(std::string & buf, const std::string & name, bool num)
{
    amf0_avl_t avl = {};
    avl.len = static_cast<int>(name.length());
    avl.data = const_cast<char *>(name.data());
    return amf0_encode_avl_boolean(buf, avl, num);
}

bool amf0_encode_string(std::string & buf, const amf0_avl_t & val)
{
    if (val.len <= 0 || nullptr == val.data)
    {
        os_rtmp_log_warn("No buffer pointer/empty buffer");
        return false;
    }

    if (val.len < USHRT_MAX)
    {
        buf.append(1, AMF0_DATA_TYPE_STRING);
        amf0_encode_int16(buf, static_cast<int16_t>(val.len));
    }
    else
    {
        buf.append(1, AMF0_DATA_TYPE_LONG_STRING);
        amf0_encode_int32be(buf, val.len);
    }
    buf.append(val.data, static_cast<size_t>(val.len));

    return true;
}

bool amf0_encode_string_ex(std::string & buf, const std::string & val)
{
    amf0_avl_t avl = { 0 };
    avl.len = static_cast<int>(val.length());
    avl.data = const_cast<char *>(val.data());
    return amf0_encode_string(buf, avl);
}

void amf0_decode_string(const char * buf, amf0_avl_t & val)
{
    val.len = amf0_decode_int16(buf);
    val.data = (val.len > 0) ? const_cast<char *>(buf) + 2 : nullptr;
}

void amf0_decode_long_string(const char * buf, amf0_avl_t & val)
{
    val.len = amf0_decode_int32be(buf);
    val.data = (val.len > 0) ? const_cast<char *>(buf) + 4 : nullptr;
}

bool amf0_encode_avl_string(std::string & buf, const amf0_avl_t & name, const amf0_avl_t & val)
{
    if (nullptr == name.data || name.len <= 0)
    {
        os_rtmp_log_warn("No buffer pointer/empty buffer");
        return false;
    }

    amf0_encode_int16(buf, name.len);
    buf.append(name.data, static_cast<size_t>(name.len));

    return amf0_encode_string(buf, val);
}

bool amf0_encode_avl_string_ex(std::string & buf, const std::string & name, const std::string & val)
{
    amf0_avl_t name_avl = {}, val_avl = {};
    name_avl.len = static_cast<int>(name.length());
    name_avl.data = const_cast<char *>(name.data());

    val_avl.len = static_cast<int>(val.length());
    val_avl.data = const_cast<char *>(val.data());

    return amf0_encode_avl_string(buf, name_avl, val_avl);
}

bool amf0_encode_strict_array(std::string & buf, const amf0_object_t & objs)
{
    buf.append(1, AMF0_DATA_TYPE_STRICT_ARRAY);
    amf0_encode_int32be(buf, objs.cnt);
    for (int i = 0; i < objs.cnt; ++i)
    {
        if (!amf0_encode_prop(buf, objs.props[i]))
            return false;
    }

    return true;
}

int amf0_decode_strict_array(const char * buf, int len, bool decode_name, amf0_object_t & obj)
{
    int ori_len = len;
    bool flag = true;
    std::vector<amf0_property_t> props;

    int cnt = amf0_decode_int32be(buf);
    
    len -= 4;
    buf += 4;

    while (cnt > 0)
    {
        cnt--;
        if (len <= 0)
        {
            flag = false;
            break;
        }

        amf0_property_t prop = { 0 };
        int res = amf0_decode_prop(buf, len, decode_name, prop);
        if (-1 == res)
        {
            flag = false;
            break;
        }
        len -= res;
        buf += res;
        props.emplace_back(prop);
    }

    if (!flag || !amf0_add_props(props, obj))
        return -1;

    return ori_len - len;
}

bool amf0_encode_ecma_array(std::string & buf, const amf0_object_t & objs)
{
    buf.append(1, AMF0_DATA_TYPE_ECMA_ARRAY);
    amf0_encode_int32be(buf, objs.cnt);

    for (int i = 0; i < objs.cnt; ++i)
    {
        if (!amf0_encode_prop(buf, objs.props[i]))
            return false;
    }
    amf0_encode_int24be(buf, AMF0_DATA_TYPE_OBJECT_END);

    return true;
}

bool amf0_encode_prop(std::string & buf, const amf0_property_t & prop)
{
    bool res = true;
    if (!amf0_encode_string(buf, prop.name))
        return false;

    switch (prop.type)
    {
    case AMF0_DATA_TYPE_NUMBER:
        amf0_encode_number(buf, prop.vu.num);
        break;
    case AMF0_DATA_TYPE_BOOLEAN:
        amf0_encode_boolean(buf, std::abs(prop.vu.num) < AMF0_DOUBLE_ZERO);
        break;
    case AMF0_DATA_TYPE_STRING:
        res = amf0_encode_string(buf, prop.vu.vl);
        break;
    case AMF0_DATA_TYPE_OBJECT:
        res = amf0_encode_object(buf, prop.vu.obj);
        break;
    case AMF0_DATA_TYPE_NULL:
        buf.append(1, AMF0_DATA_TYPE_NULL);
        break;
    case AMF0_DATA_TYPE_ECMA_ARRAY:
        res = amf0_encode_ecma_array(buf, prop.vu.obj);
        break;
    case AMF0_DATA_TYPE_STRICT_ARRAY:
        res = amf0_encode_strict_array(buf, prop.vu.obj);
        break;
    default:
        res = false;
        break;
    }

    return res;
}

int amf0_decode_prop(const char * buf, int len, bool decode_name, amf0_property_t & prop)
{
    int ori_len = len;

    if (nullptr == buf || len <= 0)
    {
        os_rtmp_log_warn("No buffer pointer/empty buffer");
        return -1;
    }

    if (decode_name && len < 4)
    {
        os_rtmp_log_warn("Not enough data for decoding with name, less than 4 bytes");
        return -1;
    }

    if (decode_name)
    {
        uint16_t name_size = amf0_decode_int16(buf);
        if (name_size > len - 2)
        {
            os_rtmp_log_warn("Name size out of range: namesize (%d) > len (%d) - 2",
                             name_size, len - 2);
            return -1;
        }

        amf0_decode_string(buf, prop.name);
        len -= (2 + name_size);
        buf += (2 + name_size);
    }

    if (0 == len)
    {
        return -1;
    }

    len--;
    prop.type = static_cast<AMF0_DATA_TYPE>(*buf++);

    switch (prop.type)
    {
    case AMF0_DATA_TYPE_NUMBER:
        if (len < 8)
            return -1;
        prop.vu.num = amf0_decode_number(buf);
        len -= 8;
        break;
    case AMF0_DATA_TYPE_BOOLEAN:
        if (len < 1)
            return -1;
        prop.vu.num = static_cast<double>(amf0_decode_boolean(buf));
        len--;
        break;
    case AMF0_DATA_TYPE_STRING:
    {
        uint16_t str_size = amf0_decode_int16(buf);
        if (len < static_cast<int>(str_size) + 2)
            return -1;
        amf0_decode_string(buf, prop.vu.vl);
        len -= (2 + str_size);
    }
        break;
    case AMF0_DATA_TYPE_OBJECT:
    {
        int ret = amf0_decode_object(buf, len, true, prop.vu.obj);
        if (-1 == ret)
            return -1;
        len -= ret;
    }
        break;
    case AMF0_DATA_TYPE_MOVIECLIP:
        os_rtmp_log_error("AMF_MOVIECLIP reserved");
        return -1;
    case AMF0_DATA_TYPE_NULL:
    case AMF0_DATA_TYPE_UNDEFINED:
    case AMF0_DATA_TYPE_UNSUPPORTED:
        prop.type = AMF0_DATA_TYPE_NULL;
        break;
    case AMF0_DATA_TYPE_REFERENCE:
        os_rtmp_log_error("AMF_REFERENCE not supported");
        return -1;
    case AMF0_DATA_TYPE_ECMA_ARRAY:
    {
        len -= 4;
        int ret = amf0_decode_object(buf + 4, len, true, prop.vu.obj);
        if (-1 == ret)
            return -1;
        len -= ret;
    }
        break;
    case AMF0_DATA_TYPE_OBJECT_END:
        return -1;
    case AMF0_DATA_TYPE_STRICT_ARRAY:
    {
        int ret = amf0_decode_strict_array(buf, len, decode_name, prop.vu.obj);
        if (-1 == ret)
            return -1;
        len -= ret;
    }
        break;
    case AMF0_DATA_TYPE_DATE:
        if (len < 10)
            return -1;
        prop.vu.num = amf0_decode_number(buf);
        prop.utc_offset = amf0_decode_int16(buf + 8);
        len -= 10;
        break;
    case AMF0_DATA_TYPE_LONG_STRING:
    case AMF0_DATA_TYPE_XML_DOCUMEN:
    {
        uint32_t str_size = amf0_decode_int32be(buf);
        if (len < static_cast<int>(str_size) + 4)
            return -1;
        amf0_decode_long_string(buf, prop.vu.vl);
        len -= (4 + str_size);
        if (AMF0_DATA_TYPE_LONG_STRING == prop.type)
            prop.type = AMF0_DATA_TYPE_STRING;
    }
        break;
    case AMF0_DATA_TYPE_RECORDSET:
        os_rtmp_log_error("AMF_RECORDSET reserved");
        return -1;
    case AMF0_DATA_TYPE_TYPED_OBJECT:
        os_rtmp_log_error("AMF_TYPED_OBJECT reserved");
        break;
    default:
        break;
    }

    return ori_len - len;
}

bool amf0_encode_object(std::string & buf, const amf0_object_t & obj)
{
    buf.append(1, AMF0_DATA_TYPE_OBJECT);
    for (int i = 0; i < obj.cnt; ++i)
    {
        if (!amf0_encode_prop(buf, obj.props[i]))
            return false;
    }
    amf0_encode_int24be(buf, AMF0_DATA_TYPE_OBJECT_END);

    return true;
}

int amf0_decode_object(const char * buf, int len, bool decode_name, amf0_object_t & obj)
{
    bool res = true;
    int ori_len = len;
    std::vector<amf0_property_t> props;

    while (len > 0)
    {
        int ret = 0;
        if (len >= 3 && AMF0_DATA_TYPE_OBJECT_END == amf0_decode_int24(buf))
        {
            len -= 3;
            res = true;
            break;
        }

        if (!res)
        {
            len--;
            buf++;
            continue;
        }

        amf0_property_t prop = { 0 };
        ret = amf0_decode_prop(buf, len, decode_name, prop);
        if (-1 == ret)
        {
            res = false;
            break;
        }
        len -= ret;
        if (len < 0)
        {
            res = false;
            break;
        }
        buf += ret;
        props.emplace_back(prop);
    }

    if (!res || !amf0_add_props(props, obj))
        return -1;

    return ori_len - len;
}

amf0_property_t * amf0_get_property(const amf0_object_t & obj, const char * key, int index)
{
    if (index >= 0)
    {
        if (index >= obj.cnt)
            return nullptr;
        return &obj.props[index];
    }

    if (nullptr == key)
        return nullptr;

    for (int i = 0; i < obj.cnt; ++i)
    {
        if (amf0_match(amf0_avl(key), obj.props[i].name))
            return &obj.props[i];
    }
    return nullptr;
}

void amf0_get_string(const amf0_property_t & prop, amf0_avl_t & val)
{
    if (AMF0_DATA_TYPE_STRING == prop.type)
        val = prop.vu.vl;
    else
        memset(&val, 0, sizeof(amf0_avl_t));
}

const char * amf0_get_string_ex(const amf0_property_t & prop)
{
    if (AMF0_DATA_TYPE_STRING != prop.type)
        return nullptr;
    return prop.vu.vl.data;
}

void amf0_prop_reset(amf0_property_t & prop)
{
    prop.type = AMF0_DATA_TYPE_INVALID;

    if (AMF0_DATA_TYPE_OBJECT == prop.type ||
        AMF0_DATA_TYPE_ECMA_ARRAY == prop.type ||
        AMF0_DATA_TYPE_STRICT_ARRAY == prop.type)
    {
        amf0_object_reset(prop.vu.obj);
        return;
    }

    prop.vu.vl.len = 0;
    prop.vu.vl.data = nullptr;
}

void amf0_object_reset(amf0_object_t & obj)
{
    for (int i = 0; i < obj.cnt; ++i)
    {
        amf0_prop_reset(obj.props[i]);
    }

    free(obj.props);
    obj.cnt = 0;
    obj.props = nullptr;
}

amf0_avl_t amf0_avl(const std::string & val)
{
    amf0_avl_t vl = {};
    vl.len = static_cast<int>(val.length());
    vl.data = const_cast<char *>(val.c_str());
    return vl;
}

bool amf0_match(const amf0_avl_t & av1, const amf0_avl_t & av2)
{
    if (av1.len == av2.len && 0 == memcmp(av1.data, av2.data, av1.len))
        return true;
    return false;
}

bool amf0_add_props(const std::vector<amf0_property_t> & props, amf0_object_t & obj)
{
    if (props.empty())
        return true;

    obj.cnt = 0;
    obj.props = static_cast<amf0_property_t *>(calloc(props.size(), sizeof(amf0_property_t)));
    if (nullptr == obj.props)
    {
        os_rtmp_log_error("No enough memory");
        return false;
    }

    for (const auto & prop : props)
    {
        memcpy(&obj.props[obj.cnt], &prop, sizeof(amf0_property_t));
        ++obj.cnt;
    }

    return true;
}
