#ifndef LSDRIVER_NETWORK_IPV4_PING_H
#define LSDRIVER_NETWORK_IPV4_PING_H

#include <linux/errno.h>
#include <linux/icmp.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/jiffies.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <net/net_namespace.h>
#include <net/sock.h>
#include <net/inet_sock.h>
#include "ipv4_dns.h"

#define IPV4_PING_DEFAULT_TIMEOUT_MS 1000U
#define IPV4_PING_PAYLOAD_SIZE 8U

struct ipv4_ping_packet {
    struct icmphdr header;
    uint8_t payload[IPV4_PING_PAYLOAD_SIZE];
};

static inline int ipv4_ping(const char *target, unsigned int timeout_ms)
{
    struct socket *socket = NULL;
    struct sockaddr_in address = {};
    struct ipv4_ping_packet request = {};
    struct ipv4_ping_packet reply = {};
    struct msghdr message = {};
    struct kvec vector;
    int received_length;
    int ret;

    if (!target || !timeout_ms) return -EINVAL;
    unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);
    ret = ipv4_dns_resolve_until(target, &address.sin_addr.s_addr, deadline);
    if (ret < 0) return ret;
    address.sin_family = AF_INET;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_ICMP, &socket);
    if (ret < 0) return ret;

    long remaining = ipv4_net_remaining(deadline);
    if (!remaining)
    {
        ret = -ETIMEDOUT;
        goto out_release;
    }
    socket->sk->sk_sndtimeo = remaining;

    request.header.type = ICMP_ECHO;
    request.header.un.echo.sequence = htons(1);
    get_random_bytes(request.payload, sizeof(request.payload));

    message.msg_name = &address;
    message.msg_namelen = sizeof(address);
    vector.iov_base = &request;
    vector.iov_len = sizeof(request);

    ret = kernel_sendmsg(socket, &message, &vector, 1, vector.iov_len);
    if (ret < 0) goto out_release;
    if (ret != vector.iov_len) {
        ret = -EIO;
        goto out_release;
    }

    request.header.un.echo.id = inet_sk(socket->sk)->inet_sport;

    for (;;) {
        remaining = ipv4_net_remaining(deadline);
        if (!remaining)
        {
            ret = -ETIMEDOUT;
            goto out_release;
        }
        socket->sk->sk_rcvtimeo = remaining;
        struct sockaddr_in source = {};
        message = (struct msghdr){.msg_name = &source, .msg_namelen = sizeof(source)};
        vector = (struct kvec){.iov_base = &reply, .iov_len = sizeof(reply)};
        received_length = kernel_recvmsg(socket, &message, &vector, 1, sizeof(reply), 0);
        if (received_length < 0) {
            ret = received_length == -EAGAIN ? -ETIMEDOUT : received_length;
            goto out_release;
        }
        if (received_length != sizeof(reply) || (message.msg_flags & MSG_TRUNC)) continue;
        if (source.sin_family != AF_INET || source.sin_addr.s_addr != address.sin_addr.s_addr) continue;
        if (reply.header.type != ICMP_ECHOREPLY || reply.header.code != 0) continue;
        if (reply.header.un.echo.id != request.header.un.echo.id || reply.header.un.echo.sequence != request.header.un.echo.sequence) continue;
        if (__builtin_memcmp(reply.payload, request.payload, sizeof(request.payload))) continue;

        ret = 0;
        goto out_release;
    }

out_release:
    sock_release(socket);
    return ret;
}

#endif
