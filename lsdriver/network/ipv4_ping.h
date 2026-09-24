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

#define IPV4_PING_DEFAULT_TIMEOUT_MS 1000U
#define IPV4_PING_PAYLOAD_SIZE 8U

struct ipv4_ping_packet {
    struct icmphdr header;
    u8 payload[IPV4_PING_PAYLOAD_SIZE];
};

static inline int ipv4_ping(const char *target, unsigned int timeout_ms)
{
    struct socket *socket = NULL;
    struct sockaddr_in address = {};
    struct ipv4_ping_packet request = {};
    struct icmphdr reply = {};
    struct msghdr message = {};
    struct kvec vector;
    u8 parsed_address[sizeof(address.sin_addr.s_addr)];
    const char *address_end = NULL;
    int received_length;
    int ret;

    if (!target || !timeout_ms) return -EINVAL;
    if (!in4_pton(target, -1, parsed_address, -1, &address_end)) return -EINVAL;
    if (address_end && *address_end != '\0') return -EINVAL;

    __builtin_memcpy(&address.sin_addr.s_addr, parsed_address, sizeof(parsed_address));
    address.sin_family = AF_INET;

    ret = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_ICMP, &socket);
    if (ret < 0) return ret;

    socket->sk->sk_rcvtimeo = msecs_to_jiffies(timeout_ms);

    request.header.type = ICMP_ECHO;
    request.header.un.echo.id = htons((u16)(current->pid & 0xffff));
    request.header.un.echo.sequence = htons(1);

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

    message = (struct msghdr){};
    vector.iov_base = &reply;
    vector.iov_len = sizeof(reply);

    for (;;) {
        received_length = kernel_recvmsg(socket, &message, &vector, 1, sizeof(reply), 0);
        if (received_length < 0) {
            ret = received_length;
            goto out_release;
        }
        if (received_length < sizeof(reply)) continue;
        if (reply.type != ICMP_ECHOREPLY || reply.code != 0) continue;
        if (reply.un.echo.id != request.header.un.echo.id || reply.un.echo.sequence != request.header.un.echo.sequence) continue;

        ret = 0;
        goto out_release;
    }

out_release:
    sock_release(socket);
    return ret;
}

#endif
