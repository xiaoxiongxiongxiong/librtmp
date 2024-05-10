#ifndef _OS_AMF0_H_
#define _OS_AMF0_H_

#include <stdint.h>
#include <stdbool.h>
#include <string>

typedef enum _AMF0_DATA_TYPE
{
    AMF0_DATA_TYPE_NUMBER = 0x00,
    AMF0_DATA_TYPE_BOOLEAN = 0x01,
    AMF0_DATA_TYPE_STRING = 0x02,
    AMF0_DATA_TYPE_OBJECT = 0x03,
    AMF0_DATA_TYPE_MOVIECLIP = 0x04,  // reserved, not supported
    AMF0_DATA_TYPE_NULL = 0x05,
    AMF0_DATA_TYPE_UNDEFINED = 0x06,
    AMF0_DATA_TYPE_REFERENCE = 0x07,
    AMF0_DATA_TYPE_ECMA_ARRAY = 0x08,
    AMF0_DATA_TYPE_OBJECT_END = 0x09,
    AMF0_DATA_TYPE_STRICT_ARRAY = 0x0A,
    AMF0_DATA_TYPE_DATE = 0x0B,
    AMF0_DATA_TYPE_LONG_STRING = 0x0C,
    AMF0_DATA_TYPE_UNSUPPORTED = 0x0D,
    AMF0_DATA_TYPE_RECORDSET = 0x0E, // reserved, not supported
    AMF0_DATA_TYPE_XML_DOCUMEN = 0x0F,
    AMF0_DATA_TYPE_TYPED_OBJECT = 0x10,
    AMF0_DATA_TYPE_INVALID = 0xff
} AMF0_DATA_TYPE;

typedef struct _amf0_avl_t
{
    char * data;
    int len;
} amf0_avl_t;

typedef struct _amf0_property_t amf0_property_t;

typedef struct _amf0_object_t
{
    int cnt;
    amf0_property_t * props;
} amf0_object_t;

typedef struct _amf0_property_t
{
    amf0_avl_t name;
    AMF0_DATA_TYPE type;
    union 
    {
        double num;
        amf0_avl_t vl;
        amf0_object_t obj;
    } vu;
    int16_t utc_offset;
} amf0_property_t;

/**
 * 
 */
void amf0_encode_int16(std::string & buf, int16_t num);

/**
 * 
 */
uint16_t amf0_decode_int16(const char * buf);

/**
 *
 */
void amf0_encode_int24be(std::string & buf, int32_t num);

/**
 * 
 */
void amf0_encode_int24le(std::string & buf, int32_t num);

/**
 * 
 */
uint32_t amf0_decode_int24(const char * buf);

/**
 *
 */
void amf0_encode_int32be(std::string & buf, int32_t num);

/**
 * 
 */
uint32_t amf0_decode_int32be(const char * buf);

/**
 * 
 */
void amf0_encode_int32le(std::string & buf, int32_t num);

/**
 * 
 */
uint32_t amf0_decode_int32le(const char * buf);

/**
 * @brief 
 * @param
 */
void amf0_encode_number(std::string & buf, double num);

/**
 * 
 */
double amf0_decode_number(const char * buf);

/**
 * 
 */
bool amf0_encode_avl_number(std::string & buf, const amf0_avl_t & name, double num);

/**
 * 
 */
bool amf0_encode_avl_number_ex(std::string & buf, const std::string & name, double num);

/**
 * @brief
 * @param
 */
void amf0_encode_boolean(std::string & buf, bool num);

/**
 * 
 */
bool amf0_decode_boolean(const char * buf);

/**
 * 
 */
bool amf0_encode_avl_boolean(std::string & buf, const amf0_avl_t & name, bool num);

/**
 * 
 */
bool amf0_encode_avl_boolean_ex(std::string & buf, const std::string & name, bool num);

/**
 * @brief
 * @param
 */
bool amf0_encode_string(std::string & buf, const amf0_avl_t & val);

/**
 * 
 */
bool amf0_encode_string_ex(std::string & buf, const std::string & val);

/**
 * 
 */
void amf0_decode_string(const char * buf, amf0_avl_t & val);

/**
 * 
 */
void amf0_decode_long_string(const char * buf, amf0_avl_t & val);

/**
 * 
 */
bool amf0_encode_avl_string(std::string & buf, const amf0_avl_t & name, const amf0_avl_t & val);

/**
 * 
 */
bool amf0_encode_avl_string_ex(std::string & buf, const std::string & name, const std::string & val);

/**
 * 
 */
bool amf0_encode_strict_array(std::string & buf, const amf0_object_t & objs);

/**
 * 
 */
int amf0_decode_strict_array(const char * buf, int len, bool decode_name, amf0_object_t & obj);

/**
 * 
 */
bool amf0_encode_ecma_array(std::string & buf, const amf0_object_t & objs);

/**
 * 
 */
bool amf0_encode_prop(std::string & buf, const amf0_property_t & prop);

/**
 * 
 */
int amf0_decode_prop(const char * buf, int len, bool decode_name, amf0_property_t & prop);

/**
 * @brief
 * @param
 */
bool amf0_encode_object(std::string & buf, const amf0_object_t & obj);

/**
 * 
 */
int amf0_decode_object(const char * buf, int len, bool decode_name, amf0_object_t & obj);

/**
 * 
 */
amf0_property_t * amf0_get_property(const amf0_object_t & obj, const char * key, int index);

/**
 * 
 */
void amf0_get_string(const amf0_property_t & prop, amf0_avl_t & val);

/**
 * 
 */
const char * amf0_get_string_ex(const amf0_property_t & prop);

/**
 *
 */
void amf0_prop_reset(amf0_property_t & prop);

/**
 * 
 */
void amf0_object_reset(amf0_object_t & obj);

/**
 * 
 */
amf0_avl_t amf0_avl(const std::string & val);

/**
 * 
 */
bool amf0_match(const amf0_avl_t & av1, const amf0_avl_t & av2);

#endif
