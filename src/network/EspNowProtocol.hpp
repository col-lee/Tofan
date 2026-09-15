#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace espnow
{
    constexpr size_t MaxPeers = 8, MaxPayload = 200, Header = 10;
    enum Type : uint8_t
    {
        Data = 1,
        Ping = 2,
        Pong = 3
    };
    inline int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                                                 : c >= 'A' && c <= 'F'   ? c - 'A' + 10
                                                                                          : -1; }
    inline bool mac(const char *s, uint8_t *out)
    {
        if (!s || strlen(s) != 17)
            return false;
        unsigned any = 0;
        for (int i = 0; i < 6; ++i)
        {
            int a = hex(s[i * 3]), b = hex(s[i * 3 + 1]);
            if (a < 0 || b < 0 || (i < 5 && s[i * 3 + 2] != ':'))
                return false;
            out[i] = (a << 4) | b;
            any |= out[i];
        }
        return any && !(out[0] & 1); // unicast only
    }
    inline size_t encode(uint8_t *out, Type type, uint32_t seq, const uint8_t *data, size_t n)
    {
        if (n > MaxPayload || (!data && n) || type < Data || type > Pong || (type != Data && n))
            return 0;
        out[0] = 'T';
        out[1] = 'F';
        out[2] = 1;
        out[3] = type;
        for (int i = 0; i < 4; ++i)
            out[4 + i] = seq >> (8 * i);
        out[8] = n;
        out[9] = n >> 8;
        if (n)
            memcpy(out + Header, data, n);
        return Header + n;
    }
    inline bool decode(const uint8_t *p, size_t n, Type &type, uint32_t &seq, size_t &size)
    {
        if (!p || n < Header || p[0] != 'T' || p[1] != 'F' || p[2] != 1 || p[3] < Data || p[3] > Pong)
            return false;
        size = p[8] | (size_t(p[9]) << 8);
        type = Type(p[3]);
        seq = 0;
        for (int i = 0; i < 4; ++i)
            seq |= uint32_t(p[4 + i]) << (8 * i);
        return size <= MaxPayload && n == Header + size && (type == Data || size == 0);
    }
    inline bool online(bool seen, uint32_t last, uint32_t now) { return seen && uint32_t(now - last) < 15000; }
}
