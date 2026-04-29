#ifndef SMSBARE_SMSPLUS_ZLIB_STUB_H
#define SMSBARE_SMSPLUS_ZLIB_STUB_H

typedef unsigned long uLong;

static inline uLong crc32(uLong crc, const unsigned char *buf, unsigned len)
{
    (void) buf;
    (void) len;
    return crc;
}

#endif
