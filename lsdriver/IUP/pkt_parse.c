// SPDX-License-Identifier: GPL-2.0
/*
 * pkt_parse.c - IPv4/IPv6 TCP/UDP/ICMP 报文解析（只读，不改包）
 *
 * 本工程编译开启 -O3 -fstrict-aliasing，因此这里不使用
 * “把 __u8 缓冲区强转成协议头结构体”的写法；
 * 所有头部统一先 memcpy 到对齐的本地结构体再读字段。
 */
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/printk.h>

#include "pkt_hook.h"

#define PKT_MAX_EXT_HDRS	8

bool pkt_trace_all;

/* 自带 IPv6 扩展头结构，避开不同内核版本头文件差异 */
struct pkt_ext_hdr {
	__u8	nexthdr;
	__u8	hdrlen;
} __packed;

struct pkt_frag_hdr {
	__u8	nexthdr;
	__u8	reserved;
	__be16	frag_off;
	__be32	identification;
} __packed;

/*
 * 把 skb 中 off 开始的 len 字节复制到对齐的本地缓冲区。
 * off 相对 skb->data，允许负数：tcp_v4_rcv / udp_rcv / icmp_rcv
 * 入口 data 已在 L4，IP 头落在 headroom 中。
 */
static bool pkt_hdr_copy(const struct sk_buff *skb, int off, void *dst, int len)
{
	const void *p;

	if (len <= 0)
		return false;

	/* 负偏移不能越过 head 起点 */
	if (off < -(int)skb_headroom(skb))
		return false;

	p = skb_header_pointer(skb, off, len, dst);
	if (!p)
		return false;

	/* linear 时返回 skb 内部指针，再拷进本地对齐结构体 */
	if (p != dst)
		memcpy(dst, p, len);

	return true;
}

static bool pkt_icmp_error4(__u8 type)
{
	switch (type) {
	case 3:
	case 4:
	case 5:
	case 11:
	case 12:
		return true;
	default:
		return false;
	}
}

/* 解析 ICMP 差错报文里引用的内层五元组 */
static void pkt_parse_inner(struct sk_buff *skb, int off, bool v6,
			    struct pkt_info *pi)
{
	__be16 sport, dport;

	if (v6) {
		struct ipv6hdr ih;
		int n = (int)sizeof(struct ipv6hdr);

		if (!pkt_hdr_copy(skb, off, &ih, (int)sizeof(ih)) ||
		    ih.version != 6)
			return;

		pi->inner.is_v6 = 1;
		pi->inner.l4proto = ih.nexthdr;
		pi->inner.saddr6 = ih.saddr;
		pi->inner.daddr6 = ih.daddr;

		if (ih.nexthdr != IPPROTO_TCP && ih.nexthdr != IPPROTO_UDP)
			goto out;

		if (pkt_hdr_copy(skb, off + n, &sport, 2) &&
		    pkt_hdr_copy(skb, off + n + 2, &dport, 2)) {
			pi->inner.sport = sport;
			pi->inner.dport = dport;
		}
	} else {
		struct iphdr ih;
		unsigned int ihl;

		if (!pkt_hdr_copy(skb, off, &ih, (int)sizeof(ih)) ||
		    ih.version != 4)
			return;

		ihl = (unsigned int)ih.ihl * 4;
		if (ihl < sizeof(struct iphdr))
			return;

		pi->inner.is_v6 = 0;
		pi->inner.l4proto = ih.protocol;
		pi->inner.saddr = ih.saddr;
		pi->inner.daddr = ih.daddr;

		if (ih.protocol != IPPROTO_TCP && ih.protocol != IPPROTO_UDP)
			goto out;

		if (pkt_hdr_copy(skb, off + (int)ihl, &sport, 2) &&
		    pkt_hdr_copy(skb, off + (int)ihl + 2, &dport, 2)) {
			pi->inner.sport = sport;
			pi->inner.dport = dport;
		}
	}

out:
	pi->has_inner = true;
}

static int pkt_parse_l4(struct sk_buff *skb, int off, __u8 proto,
			struct pkt_info *pi)
{
	int total = (int)skb->len;

	if (off < 0 || off > total)
		return -EINVAL;

	pi->tu.l4proto = proto;

	switch (proto) {
	case IPPROTO_TCP: {
		struct tcphdr th;
		unsigned int doff;

		if (!pkt_hdr_copy(skb, off, &th, (int)sizeof(th)))
			return -EINVAL;

		pi->tu.sport = th.source;
		pi->tu.dport = th.dest;
		pi->tcp_seq = th.seq;
		pi->tcp_ack = th.ack_seq;
		/* 不依赖 <net/tcp.h> 的 tcp_flag_byte：flags 在 TCP 头第 13 字节 */
		pi->tcp_flags = ((const __u8 *)&th)[13];

		doff = (unsigned int)th.doff * 4;
		if (doff >= sizeof(struct tcphdr) &&
		    off + (int)doff <= total)
			pi->payload_len = total - off - (int)doff;
		break;
	}
	case IPPROTO_UDP: {
		struct udphdr uh;
		unsigned int ulen;
		int avail;

		if (!pkt_hdr_copy(skb, off, &uh, (int)sizeof(uh)))
			return -EINVAL;

		pi->tu.sport = uh.source;
		pi->tu.dport = uh.dest;

		ulen = ntohs(uh.len);
		ulen = ulen > sizeof(struct udphdr)
		     ? ulen - sizeof(struct udphdr) : 0;

		avail = total - off - (int)sizeof(struct udphdr);
		if (avail < 0)
			avail = 0;
		if ((int)ulen > avail)
			ulen = (unsigned int)avail;
		pi->payload_len = ulen;
		break;
	}
	case IPPROTO_ICMP: {
		struct icmphdr ih;

		if (!pkt_hdr_copy(skb, off, &ih, (int)sizeof(ih)))
			return -EINVAL;

		pi->icmp_type = ih.type;
		pi->icmp_code = ih.code;

		if (total > off + (int)sizeof(ih)) {
			pi->payload_len = total - off - (int)sizeof(ih);
			if (pkt_icmp_error4(ih.type))
				pkt_parse_inner(skb, off + (int)sizeof(ih),
						false, pi);
		}
		break;
	}
	case IPPROTO_ICMPV6: {
		struct icmp6hdr ih;

		if (!pkt_hdr_copy(skb, off, &ih, (int)sizeof(ih)))
			return -EINVAL;

		pi->icmp_type = ih.icmp6_type;
		pi->icmp_code = ih.icmp6_code;

		if (total > off + (int)sizeof(ih)) {
			pi->payload_len = total - off - (int)sizeof(ih);
			if (ih.icmp6_type < 128)
				pkt_parse_inner(skb, off + (int)sizeof(ih),
						true, pi);
		}
		break;
	}
	default:
		if (total > off)
			pi->payload_len = total - off;
		break;
	}

	return 0;
}

static int pkt_parse4(struct sk_buff *skb, int nh_off, struct pkt_info *pi)
{
	struct iphdr iph;
	unsigned int ihl;
	__u16 frag;
	int l4_off;

	if (!pkt_hdr_copy(skb, nh_off, &iph, (int)sizeof(iph)) ||
	    iph.version != 4)
		return -EPROTO;

	ihl = (unsigned int)iph.ihl * 4;
	if (ihl < sizeof(struct iphdr))
		return -EPROTO;

	pi->tu.is_v6 = 0;
	pi->tu.saddr = iph.saddr;
	pi->tu.daddr = iph.daddr;
	pi->ip_id = ntohs(iph.id);
	pi->ttl = iph.ttl;

	frag = ntohs(iph.frag_off);
	if (frag & 0x2000)
		pi->frag = true;
	if (frag & 0x1fff)		/* 非首片，无 L4 头 */
		return 0;

	l4_off = nh_off + (int)ihl;
	return pkt_parse_l4(skb, l4_off, iph.protocol, pi);
}

static int pkt_parse6(struct sk_buff *skb, int nh_off, struct pkt_info *pi)
{
	struct ipv6hdr ip6h;
	int off;
	__u8 nexthdr;
	int i;

	if (!pkt_hdr_copy(skb, nh_off, &ip6h, (int)sizeof(ip6h)) ||
	    ip6h.version != 6)
		return -EPROTO;

	pi->tu.is_v6 = 1;
	pi->tu.saddr6 = ip6h.saddr;
	pi->tu.daddr6 = ip6h.daddr;
	pi->ttl = ip6h.hop_limit;

	nexthdr = ip6h.nexthdr;
	off = nh_off + (int)sizeof(struct ipv6hdr);

	for (i = 0; i < PKT_MAX_EXT_HDRS; i++) {
		if (nexthdr == IPPROTO_HOPOPTS ||
		    nexthdr == IPPROTO_ROUTING ||
		    nexthdr == IPPROTO_DSTOPTS) {
			struct pkt_ext_hdr eh;

			if (!pkt_hdr_copy(skb, off, &eh, (int)sizeof(eh)))
				return -EINVAL;

			nexthdr = eh.nexthdr;
			off += (eh.hdrlen + 1) * 8;
		} else if (nexthdr == IPPROTO_FRAGMENT) {
			struct pkt_frag_hdr fh;

			if (!pkt_hdr_copy(skb, off, &fh, (int)sizeof(fh)))
				return -EINVAL;

			pi->frag = true;
			if (ntohs(fh.frag_off) & 0xfff8)
				return 0;	/* 非首片 */

			nexthdr = fh.nexthdr;
			off += (int)sizeof(struct pkt_frag_hdr);
		} else {
			break;
		}
	}

	return pkt_parse_l4(skb, off, nexthdr, pi);
}

int pkt_inspect(struct sk_buff *skb, __u8 dir, struct pkt_info *pi)
{
	__u8 ver_byte = 0;
	int nh_off;
	__u8 ver;

	if (!skb || !pi)
		return -EINVAL;

	memset(pi, 0, sizeof(*pi));
	pi->dir = dir;
	pi->pkt_len = skb->len;
	pi->ts_ns = ktime_get_ns();

	if (skb->dev) {
		pi->ifindex = skb->dev->ifindex;
		memcpy(pi->ifname, skb->dev->name, PKT_IFNAME_LEN - 1);
		pi->ifname[PKT_IFNAME_LEN - 1] = '\0';
	}

	nh_off = (int)(skb_network_header(skb) - skb->data);
	pi->l3_off = nh_off;

	if (!pkt_hdr_copy(skb, nh_off, &ver_byte, 1))
		return -EINVAL;

	ver = ver_byte >> 4;
	if (ver == 4)
		return pkt_parse4(skb, nh_off, pi);
	if (ver == 6)
		return pkt_parse6(skb, nh_off, pi);

	return -EPROTO;
}

void pkt_trace(const struct pkt_info *pi, const char *tag)
{
	const char *proto;
	const char *t = tag ? tag : "-";

	if (!pi)
		return;

	switch (pi->tu.l4proto) {
	case IPPROTO_TCP:	proto = "tcp"; break;
	case IPPROTO_UDP:	proto = "udp"; break;
	case IPPROTO_ICMP:	proto = "icmp"; break;
	case IPPROTO_ICMPV6:	proto = "icmp6"; break;
	default:		proto = "ip"; break;
	}

	if (pi->tu.l4proto == IPPROTO_ICMP ||
	    pi->tu.l4proto == IPPROTO_ICMPV6) {
		if (pi->tu.is_v6)
			pr_info_ratelimited("pkt[%s] %s v6 %pI6c > %pI6c type=%u code=%u len=%u if=%s\n",
				t, proto, &pi->tu.saddr6, &pi->tu.daddr6,
				pi->icmp_type, pi->icmp_code,
				pi->pkt_len, pi->ifname);
		else
			pr_info_ratelimited("pkt[%s] %s v4 %pI4 > %pI4 type=%u code=%u len=%u if=%s\n",
				t, proto, &pi->tu.saddr, &pi->tu.daddr,
				pi->icmp_type, pi->icmp_code,
				pi->pkt_len, pi->ifname);
		return;
	}

	if (pi->tu.is_v6)
		pr_info_ratelimited("pkt[%s] %s v6 %pI6c.%u > %pI6c.%u len=%u if=%s\n",
			t, proto, &pi->tu.saddr6, ntohs(pi->tu.sport),
			&pi->tu.daddr6, ntohs(pi->tu.dport),
			pi->pkt_len, pi->ifname);
	else
		pr_info_ratelimited("pkt[%s] %s v4 %pI4.%u > %pI4.%u len=%u if=%s\n",
			t, proto, &pi->tu.saddr, ntohs(pi->tu.sport),
			&pi->tu.daddr, ntohs(pi->tu.dport),
			pi->pkt_len, pi->ifname);
}
