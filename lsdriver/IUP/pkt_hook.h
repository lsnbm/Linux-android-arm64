/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _PKT_HOOK_H
#define _PKT_HOOK_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/in6.h>

#define PKT_DIR_IN		0u
#define PKT_DIR_OUT		1u
#define PKT_DIR_FWD		2u

#define PKT_IFNAME_LEN		16

struct pkt_tuple {
	__u8		is_v6;
	__u8		l4proto;
	__be16		sport;
	__be16		dport;
	__be32		saddr;
	__be32		daddr;
	struct in6_addr	saddr6;
	struct in6_addr	daddr6;
};

struct pkt_info {
	struct pkt_tuple	tu;
	struct pkt_tuple	inner;		/* ICMP 差错报文里的内层五元组 */
	bool			has_inner;

	__u8	tcp_flags;
	__be32	tcp_seq;
	__be32	tcp_ack;
	__u8	icmp_type;
	__u8	icmp_code;

	__u16	ip_id;
	__u8	ttl;
	bool	frag;
	__u32	payload_len;
	__u32	pkt_len;
	int	l3_off;			/* network header 相对 skb->data 的带符号偏移 */
	int	ifindex;
	char	ifname[PKT_IFNAME_LEN];
	__u64	ts_ns;
	__u8	dir;
};

int  pkt_inspect(struct sk_buff *skb, __u8 dir, struct pkt_info *pi);
void pkt_trace(const struct pkt_info *pi, const char *tag);

extern bool pkt_trace_all;

#endif /* _PKT_HOOK_H */