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
#define IPV4_PING_PAYLOAD_SIZE       8U

enum ipv4_ping_failure
{
    ipv4_ping_failure_none = 0,
    ipv4_ping_failure_invalid_argument,
    ipv4_ping_failure_resolve,
    ipv4_ping_failure_socket_create,
    ipv4_ping_failure_send_timeout,
    ipv4_ping_failure_send,
    ipv4_ping_failure_send_length,
    ipv4_ping_failure_receive_timeout,
    ipv4_ping_failure_receive,
};

enum ipv4_ping_reply_mismatch
{
    ipv4_ping_reply_mismatch_none = 0,
    ipv4_ping_reply_mismatch_length,
    ipv4_ping_reply_mismatch_source,
    ipv4_ping_reply_mismatch_type,
    ipv4_ping_reply_mismatch_identity,
    ipv4_ping_reply_mismatch_payload,
};

struct ipv4_ping_diagnostic
{
    enum ipv4_ping_failure failure;
    enum ipv4_ping_reply_mismatch reply_mismatch;
};

static inline const char *ipv4_ping_failure_name(enum ipv4_ping_failure failure)
{
    switch (failure)
    {
    case ipv4_ping_failure_none:
        return "none";
    case ipv4_ping_failure_invalid_argument:
        return "invalid_argument";
    case ipv4_ping_failure_resolve:
        return "resolve";
    case ipv4_ping_failure_socket_create:
        return "socket_create";
    case ipv4_ping_failure_send_timeout:
        return "send_timeout";
    case ipv4_ping_failure_send:
        return "send";
    case ipv4_ping_failure_send_length:
        return "send_length";
    case ipv4_ping_failure_receive_timeout:
        return "receive_timeout";
    case ipv4_ping_failure_receive:
        return "receive";
    default:
        return "unknown";
    }
}

static inline const char *ipv4_ping_reply_mismatch_name(enum ipv4_ping_reply_mismatch mismatch)
{
    switch (mismatch)
    {
    case ipv4_ping_reply_mismatch_none:
        return "none";
    case ipv4_ping_reply_mismatch_length:
        return "length";
    case ipv4_ping_reply_mismatch_source:
        return "source";
    case ipv4_ping_reply_mismatch_type:
        return "type";
    case ipv4_ping_reply_mismatch_identity:
        return "identity";
    case ipv4_ping_reply_mismatch_payload:
        return "payload";
    default:
        return "unknown";
    }
}

struct ipv4_ping_packet
{
    struct icmphdr header;
    uint8_t payload[IPV4_PING_PAYLOAD_SIZE];
};

static inline int ipv4_ping_detailed(const char *target, unsigned int timeout_ms, struct ipv4_ping_diagnostic *diagnostic)
{
    struct socket *socket = NULL;
    struct sockaddr_in address = {};
    struct ipv4_ping_packet request = {};
    struct ipv4_ping_packet reply = {};
    struct msghdr message = {};
    struct kvec vector;
    int received_length;
    int ret;

    if (diagnostic) *diagnostic = (struct ipv4_ping_diagnostic){};

    if (!target || !timeout_ms)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_invalid_argument;
        return -EINVAL;
    }
    unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);
    ret = ipv4_dns_resolve_until(target, &address.sin_addr.s_addr, deadline);
    if (ret < 0)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_resolve;
        return ret;
    }
    address.sin_family = AF_INET;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_ICMP, &socket);
    if (ret < 0)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_socket_create;
        return ret;
    }

    long remaining = ipv4_net_remaining(deadline);
    if (!remaining)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_send_timeout;
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
    if (ret < 0)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_send;
        goto out_release;
    }
    if (ret != vector.iov_len)
    {
        if (diagnostic) diagnostic->failure = ipv4_ping_failure_send_length;
        ret = -EIO;
        goto out_release;
    }

    request.header.un.echo.id = inet_sk(socket->sk)->inet_sport;

    for (;;)
    {
        remaining = ipv4_net_remaining(deadline);
        if (!remaining)
        {
            if (diagnostic) diagnostic->failure = ipv4_ping_failure_receive_timeout;
            ret = -ETIMEDOUT;
            goto out_release;
        }
        socket->sk->sk_rcvtimeo = remaining;
        struct sockaddr_in source = {};
        message = (struct msghdr){.msg_name = &source, .msg_namelen = sizeof(source)};
        vector = (struct kvec){.iov_base = &reply, .iov_len = sizeof(reply)};
        received_length = kernel_recvmsg(socket, &message, &vector, 1, sizeof(reply), 0);
        if (received_length < 0)
        {
            if (received_length == -EAGAIN)
            {
                if (diagnostic) diagnostic->failure = ipv4_ping_failure_receive_timeout;
                ret = -ETIMEDOUT;
            }
            else
            {
                if (diagnostic) diagnostic->failure = ipv4_ping_failure_receive;
                ret = received_length;
            }
            goto out_release;
        }
        if (received_length != sizeof(reply) || (message.msg_flags & MSG_TRUNC))
        {
            if (diagnostic) diagnostic->reply_mismatch = ipv4_ping_reply_mismatch_length;
            continue;
        }
        if (source.sin_family != AF_INET || source.sin_addr.s_addr != address.sin_addr.s_addr)
        {
            if (diagnostic) diagnostic->reply_mismatch = ipv4_ping_reply_mismatch_source;
            continue;
        }
        if (reply.header.type != ICMP_ECHOREPLY || reply.header.code != 0)
        {
            if (diagnostic) diagnostic->reply_mismatch = ipv4_ping_reply_mismatch_type;
            continue;
        }

        ret = 0;
        goto out_release;
    }

out_release:
    sock_release(socket);
    return ret;
}

static inline int ipv4_ping(const char *target, unsigned int timeout_ms)
{
    return ipv4_ping_detailed(target, timeout_ms, NULL);
}

#endif
