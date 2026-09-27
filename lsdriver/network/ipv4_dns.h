#ifndef LSDRIVER_NETWORK_IPV4_DNS_H
#define LSDRIVER_NETWORK_IPV4_DNS_H

#include <linux/inet.h>
#include <linux/in.h>
#include <linux/jiffies.h>
#include <linux/net.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uio.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include "dns_wire.h"

#define IPV4_DNS_PRIMARY "8.8.8.8"
#define IPV4_DNS_SECONDARY "8.8.4.4"
#define IPV4_DNS_RESPONSE_SIZE 65535U

static inline long ipv4_net_remaining(unsigned long deadline)
{
    unsigned long now = jiffies;
    return time_before(now, deadline) ? (long)(deadline - now) : 0;
}

static inline int ipv4_dns_transfer(struct socket *socket, uint8_t *data, size_t length,
                                    bool sending, unsigned long deadline)
{
    size_t done = 0;
    while (done < length)
    {
        long remaining = ipv4_net_remaining(deadline);
        if (!remaining) return -ETIMEDOUT;
        WRITE_ONCE(socket->sk->sk_sndtimeo, remaining);
        WRITE_ONCE(socket->sk->sk_rcvtimeo, remaining);
        struct msghdr message = {.msg_flags = sending ? MSG_NOSIGNAL : 0};
        struct kvec vector = {.iov_base = data + done, .iov_len = length - done};
        int status = sending ? kernel_sendmsg(socket, &message, &vector, 1, vector.iov_len) :
                               kernel_recvmsg(socket, &message, &vector, 1, vector.iov_len, 0);
        if (status < 0) return status == -EAGAIN ? -ETIMEDOUT : status;
        if (!status) return -ECONNRESET;
        done += status;
    }
    return 0;
}

static inline int ipv4_dns_exchange(const char *server, uint8_t *query, size_t query_length,
                                    uint8_t *response, bool tcp, unsigned long deadline)
{
    struct sockaddr_in destination = {.sin_family = AF_INET, .sin_port = htons(53)};
    if (!in4_pton(server, -1, (uint8_t *)&destination.sin_addr.s_addr, '\0', NULL)) return -EINVAL;
    long remaining = ipv4_net_remaining(deadline);
    if (!remaining) return -ETIMEDOUT;
    struct socket *socket = NULL;
    int status = sock_create_kern(&init_net, AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM,
                                  tcp ? IPPROTO_TCP : IPPROTO_UDP, &socket);
    if (status < 0) return status;
    WRITE_ONCE(socket->sk->sk_sndtimeo, remaining);
    status = kernel_connect(socket, (struct sockaddr *)&destination, sizeof(destination), 0);
    if (status < 0) goto out;

    if (tcp)
    {
        uint8_t prefix[2];
        dns_write16(prefix, query_length);
        status = ipv4_dns_transfer(socket, prefix, sizeof(prefix), true, deadline);
        if (status < 0) goto out;
        status = ipv4_dns_transfer(socket, query, query_length, true, deadline);
        if (status < 0) goto out;
        status = ipv4_dns_transfer(socket, prefix, sizeof(prefix), false, deadline);
        if (status < 0) goto out;
        unsigned int response_length = dns_read16(prefix);
        if (response_length < DNS_HEADER_SIZE)
        {
            status = -EBADMSG;
            goto out;
        }
        status = ipv4_dns_transfer(socket, response, response_length, false, deadline);
        if (!status) status = response_length;
        goto out;
    }

    struct msghdr message = {.msg_flags = MSG_NOSIGNAL};
    struct kvec vector = {.iov_base = query, .iov_len = query_length};
    status = kernel_sendmsg(socket, &message, &vector, 1, query_length);
    if (status < 0) goto out;
    if ((size_t)status != query_length)
    {
        status = -EIO;
        goto out;
    }
    for (;;)
    {
        remaining = ipv4_net_remaining(deadline);
        if (!remaining)
        {
            status = -ETIMEDOUT;
            break;
        }
        WRITE_ONCE(socket->sk->sk_rcvtimeo, remaining);
        message = (struct msghdr){};
        vector = (struct kvec){.iov_base = response, .iov_len = IPV4_DNS_RESPONSE_SIZE};
        status = kernel_recvmsg(socket, &message, &vector, 1, vector.iov_len, 0);
        if (status < 0) break;
        if (status < DNS_HEADER_SIZE || dns_read16(response) != dns_read16(query)) continue;
        if (!(dns_read16(response + 2) & 0x8000) || (dns_read16(response + 2) & 0x7800)) continue;
        if (message.msg_flags & MSG_TRUNC) status = -EMSGSIZE;
        break;
    }
out:
    sock_release(socket);
    return status == -EAGAIN || status == -EINPROGRESS ? -ETIMEDOUT : status;
}

struct ipv4_dns_workspace
{
    uint8_t query[DNS_QUERY_SIZE];
    uint8_t question[DNS_NAME_SIZE];
    uint8_t next_name[DNS_NAME_SIZE];
    uint8_t response[IPV4_DNS_RESPONSE_SIZE];
};

static inline int ipv4_dns_resolve_until(const char *host, __be32 *address, unsigned long deadline)
{
    if (!host || !address) return -EINVAL;
    size_t length = strnlen(host, DNS_NAME_SIZE + 1);
    if (!length || length > DNS_NAME_SIZE - 1) return -EINVAL;
    __be32 parsed;
    const char *end = NULL;
    if (in4_pton(host, length, (uint8_t *)&parsed, '\0', &end) && end == host + length)
    {
        *address = parsed;
        return 0;
    }
    struct ipv4_dns_workspace *workspace = kzalloc(sizeof(*workspace), GFP_KERNEL);
    if (!workspace) return -ENOMEM;
    int status = dns_encode_name(host, workspace->question);
    if (status < 0) goto out;
    const char *const servers[] = {IPV4_DNS_PRIMARY, IPV4_DNS_SECONDARY};
    unsigned int aliases = 0;
    for (;;)
    {
        uint16_t id;
        get_random_bytes(&id, sizeof(id));
        int query_length = dns_make_query(workspace->question, id, workspace->query);
        if (query_length < 0)
        {
            status = query_length;
            break;
        }
        for (unsigned int server_index = 0; server_index < 2; server_index++)
        {
            long remaining = ipv4_net_remaining(deadline);
            if (!remaining)
            {
                status = -ETIMEDOUT;
                break;
            }
            unsigned long attempt_deadline = server_index ? deadline : jiffies + (remaining + 1) / 2;
            unsigned int attempt_aliases = aliases;
            int received = ipv4_dns_exchange(servers[server_index], workspace->query, query_length,
                                             workspace->response, false, attempt_deadline);
            status = received < 0 ? received : dns_parse_answer(workspace->response, received, id,
                       workspace->question, workspace->next_name, &attempt_aliases, (uint8_t *)&parsed);
            if (status == -EMSGSIZE)
            {
                received = ipv4_dns_exchange(servers[server_index], workspace->query, query_length,
                                             workspace->response, true, attempt_deadline);
                status = received < 0 ? received : dns_parse_answer(workspace->response, received, id,
                           workspace->question, workspace->next_name, &attempt_aliases, (uint8_t *)&parsed);
            }
            if (!status || status == -EINPROGRESS)
            {
                aliases = attempt_aliases;
                break;
            }
            if (status == -ENOENT || status == -ENODATA || status == -ELOOP ||
                status == -EINTR || status == -ERESTARTSYS) break;
        }
        if (status != -EINPROGRESS) break;
        __builtin_memcpy(workspace->question, workspace->next_name, DNS_NAME_SIZE);
    }
    if (!status) *address = parsed;
out:
    kfree(workspace);
    return status;
}

static inline int ipv4_dns_resolve(const char *host, __be32 *address, unsigned int timeout_ms)
{
    if (!timeout_ms) return -EINVAL;
    return ipv4_dns_resolve_until(host, address, jiffies + msecs_to_jiffies(timeout_ms));
}

#endif