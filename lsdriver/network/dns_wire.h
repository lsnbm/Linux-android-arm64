#ifndef LSDRIVER_NETWORK_DNS_WIRE_H
#define LSDRIVER_NETWORK_DNS_WIRE_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/errno.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#endif

#define DNS_NAME_SIZE 255U
#define DNS_HEADER_SIZE 12U
#define DNS_QUERY_SIZE (DNS_HEADER_SIZE + DNS_NAME_SIZE + 4U)
#define DNS_CNAME_LIMIT 8U

static inline uint16_t dns_read16(const uint8_t *bytes)
{
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static inline void dns_write16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = value >> 8;
    bytes[1] = value & 0xff;
}

static inline uint8_t dns_lower(uint8_t value)
{
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static inline int dns_encode_name(const char *host, uint8_t *name)
{
    if (!host || !*host) return -EINVAL;
    size_t used = 0;
    while (*host)
    {
        if (used >= DNS_NAME_SIZE - 1) return -ENAMETOOLONG;
        size_t label = used++;
        while (*host && *host != '.')
        {
            uint8_t value = (uint8_t)*host++;
            if (!((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                  (value >= '0' && value <= '9') || value == '-')) return -EINVAL;
            if (used - label > 63 || used >= DNS_NAME_SIZE - 1) return -ENAMETOOLONG;
            name[used++] = dns_lower(value);
        }
        size_t length = used - label - 1;
        if (!length || name[label + 1] == '-' || name[used - 1] == '-') return -EINVAL;
        name[label] = length;
        if (*host == '.') host++;
    }
    name[used++] = 0;
    return used;
}

static inline int dns_decode_name(const uint8_t *packet, size_t length, size_t *offset, uint8_t *name)
{
    size_t cursor = *offset;
    size_t consumed = 0;
    size_t used = 0;
    size_t ceiling = length;
    bool jumped = false;
    for (size_t steps = 0; steps < DNS_NAME_SIZE; steps++)
    {
        if (cursor >= length || cursor >= ceiling) return -EBADMSG;
        uint8_t count = packet[cursor++];
        if (!jumped) consumed++;
        if ((count & 0xc0) == 0xc0)
        {
            if (cursor >= length || cursor >= ceiling) return -EBADMSG;
            size_t target = ((size_t)(count & 0x3f) << 8) | packet[cursor];
            if (target < DNS_HEADER_SIZE || target >= cursor - 1) return -EBADMSG;
            if (!jumped) consumed++;
            ceiling = cursor - 1;
            cursor = target;
            jumped = true;
            continue;
        }
        if (count & 0xc0) return -EBADMSG;
        if (used + 1 + count > DNS_NAME_SIZE || count > length - cursor || count > ceiling - cursor) return -EBADMSG;
        name[used++] = count;
        if (!count)
        {
            *offset += consumed;
            return used;
        }
        for (unsigned int index = 0; index < count; index++) name[used++] = dns_lower(packet[cursor++]);
        if (!jumped) consumed += count;
    }
    return -ELOOP;
}

static inline bool dns_name_equal(const uint8_t *left, const uint8_t *right)
{
    size_t offset = 0;
    while (offset < DNS_NAME_SIZE)
    {
        unsigned int count = left[offset];
        if (count != right[offset] || count > 63 || offset + count >= DNS_NAME_SIZE) return false;
        if (!count) return true;
        for (unsigned int index = 1; index <= count; index++)
            if (dns_lower(left[offset + index]) != dns_lower(right[offset + index])) return false;
        offset += count + 1;
    }
    return false;
}

static inline int dns_make_query(const uint8_t *name, uint16_t id, uint8_t *query)
{
    size_t name_length = 0;
    do
    {
        unsigned int count = name[name_length];
        if (count > 63 || name_length + count >= DNS_NAME_SIZE) return -EINVAL;
        name_length += count + 1;
        if (!count) break;
        if (name_length >= DNS_NAME_SIZE) return -EINVAL;
    } while (true);
    __builtin_memset(query, 0, DNS_HEADER_SIZE);
    dns_write16(query, id);
    dns_write16(query + 2, 0x0100);
    dns_write16(query + 4, 1);
    __builtin_memcpy(query + DNS_HEADER_SIZE, name, name_length);
    dns_write16(query + DNS_HEADER_SIZE + name_length, 1);
    dns_write16(query + DNS_HEADER_SIZE + name_length + 2, 1);
    return DNS_HEADER_SIZE + name_length + 4;
}

static inline int dns_parse_answer(const uint8_t *packet, size_t length, uint16_t id,
                                   const uint8_t *question, uint8_t *next_name,
                                   unsigned int *aliases, uint8_t *address)
{
    if (length < DNS_HEADER_SIZE) return -EBADMSG;
    uint16_t flags = dns_read16(packet + 2);
    if (dns_read16(packet) != id || !(flags & 0x8000) || (flags & 0x7800)) return -EAGAIN;
    if (dns_read16(packet + 4) != 1) return -EBADMSG;
    size_t offset = DNS_HEADER_SIZE;
    uint8_t name[DNS_NAME_SIZE] = {};
    int status = dns_decode_name(packet, length, &offset, name);
    if (status < 0) return status;
    if (!dns_name_equal(name, question)) return -EAGAIN;
    if (length - offset < 4 || dns_read16(packet + offset) != 1 || dns_read16(packet + offset + 2) != 1) return -EBADMSG;
    offset += 4;
    if (flags & 0x0200) return -EMSGSIZE;
    if ((flags & 0x000f) == 3) return -ENOENT;
    if (flags & 0x000f) return -EREMOTEIO;

    size_t answers_offset = offset;
    unsigned int answers = dns_read16(packet + 6);
    unsigned int records = answers + dns_read16(packet + 8) + dns_read16(packet + 10);
    for (unsigned int index = 0; index < records; index++)
    {
        status = dns_decode_name(packet, length, &offset, name);
        if (status < 0) return status;
        if (length - offset < 10) return -EBADMSG;
        uint16_t type = dns_read16(packet + offset);
        size_t data_length = dns_read16(packet + offset + 8);
        offset += 10;
        if (data_length > length - offset) return -EBADMSG;
        if (type == 1 && data_length != 4) return -EBADMSG;
        if (type == 5)
        {
            size_t cname_offset = offset;
            status = dns_decode_name(packet, length, &cname_offset, name);
            if (status < 0 || cname_offset != offset + data_length) return -EBADMSG;
        }
        offset += data_length;
    }
    if (offset != length) return -EBADMSG;

    __builtin_memcpy(next_name, question, DNS_NAME_SIZE);
    for (;;)
    {
        bool found_alias = false;
        uint8_t alias[DNS_NAME_SIZE] = {};
        bool found_address = false;
        uint8_t candidate[4];
        offset = answers_offset;
        for (unsigned int index = 0; index < answers; index++)
        {
            status = dns_decode_name(packet, length, &offset, name);
            if (status < 0) return status;
            uint16_t type = dns_read16(packet + offset);
            uint16_t dns_class = dns_read16(packet + offset + 2);
            size_t data_length = dns_read16(packet + offset + 8);
            offset += 10;
            if (dns_class == 1 && dns_name_equal(name, next_name))
            {
                if (type == 1 && !found_address)
                {
                    __builtin_memcpy(candidate, packet + offset, 4);
                    found_address = true;
                }
                if (type == 5)
                {
                    size_t cname_offset = offset;
                    status = dns_decode_name(packet, length, &cname_offset, name);
                    if (status < 0) return status;
                    if (found_alias && !dns_name_equal(alias, name)) return -EBADMSG;
                    __builtin_memcpy(alias, name, status);
                    found_alias = true;
                }
            }
            offset += data_length;
        }
        if (found_alias)
        {
            if (found_address) return -EBADMSG;
            if (*aliases >= DNS_CNAME_LIMIT || dns_name_equal(next_name, alias)) return -ELOOP;
            (*aliases)++;
            __builtin_memcpy(next_name, alias, DNS_NAME_SIZE);
            continue;
        }
        if (found_address)
        {
            __builtin_memcpy(address, candidate, 4);
            return 0;
        }
        return dns_name_equal(next_name, question) ? -ENODATA : -EINPROGRESS;
    }
}

#endif