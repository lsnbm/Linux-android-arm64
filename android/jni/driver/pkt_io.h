#ifndef PKT_IO_H
#define PKT_IO_H

/*
 * 内核和用户态共用 ABI 头。
 * 内核侧由 __KERNEL__ 选 linux/types.h，用户态选 stdint.h。
 */

#ifdef __KERNEL__
#include <linux/types.h>
typedef __u8  pkt_u8;
typedef __u16 pkt_u16;
typedef __u32 pkt_u32;
typedef __u64 pkt_u64;
#else
#include <stdint.h>
typedef uint8_t  pkt_u8;
typedef uint16_t pkt_u16;
typedef uint32_t pkt_u32;
typedef uint64_t pkt_u64;
#endif

#define PKT_USER_BUF_SIZE	65536
#define PKT_REC_SNAPLEN		1600	/* 覆盖标准 MTU 1500 的完整 IP 包 */

/* match_flags */
#define PKT_MATCH_SADDR		(1u << 0)	/* 源 IP == saddr */
#define PKT_MATCH_DADDR		(1u << 1)	/* 目的 IP == daddr */
#define PKT_MATCH_SPORT		(1u << 2)	/* 源端口 == sport */
#define PKT_MATCH_DPORT		(1u << 3)	/* 目的端口 == dport */
#define PKT_MATCH_PEER		(1u << 4)	/* 源或目的 IP == daddr */
#define PKT_MATCH_PEER_PORT	(1u << 5)	/* 源或目的端口 == dport */

/* proto_mask */
#define PKT_PROTO_TCP_BIT	(1u << 0)
#define PKT_PROTO_UDP_BIT	(1u << 1)
#define PKT_PROTO_ICMP_BIT	(1u << 2)	/* ICMP + ICMPv6 */

/* dir_mask */
#define PKT_DIR_IN_BIT		(1u << 0)
#define PKT_DIR_OUT_BIT		(1u << 1)
#define PKT_DIR_FWD_BIT		(1u << 2)

/* action */
#define PKT_ACTION_PASS		0
#define PKT_ACTION_DROP		1

/*
 * 抓包记录。地址、端口均为网络字节序：
 *   v4 地址用 inet_addr()，端口用 htons()。
 * 固定 1680 字节（80 字节头 + 1600 字节快照 + 4 字节保留），
 * 用户态和内核必须使用同一份定义。
 */
struct pkt_record {
	pkt_u64	ts_ns;
	pkt_u32	saddr;
	pkt_u32	daddr;
	pkt_u8	saddr6[16];
	pkt_u8	daddr6[16];
	pkt_u16	sport;
	pkt_u16	dport;
	pkt_u8	proto;		/* 6/17/1/58 */
	pkt_u8	is_v6;
	pkt_u8	dir;		/* 0 in, 1 out, 2 fwd */
	pkt_u8	tcp_flags;
	pkt_u8	icmp_type;
	pkt_u8	icmp_code;
	pkt_u8	frag;
	pkt_u8	reserved0;
	pkt_u32	pkt_len;
	pkt_u32	cap_len;
	pkt_u32	ifindex;
	pkt_u32	reserved1;
	pkt_u8	data[PKT_REC_SNAPLEN];
	pkt_u8	reserved2[4];
};

struct net_filter_info {
	pkt_u32	enable;
	pkt_u32	action;		/* PKT_ACTION_PASS / PKT_ACTION_DROP */
	pkt_u32	capture;	/* 1 = 命中包写入环形缓冲 */
	pkt_u32	proto_mask;	/* PKT_PROTO_*_BIT，0 = 不限 */
	pkt_u32	dir_mask;	/* PKT_DIR_*_BIT，0 = 不限 */
	pkt_u32	match_flags;	/* PKT_MATCH_* */
	pkt_u32	is_v6;
	pkt_u32	saddr[4];	/* v4 用 [0]，v6 用满 16 字节 */
	pkt_u32	daddr[4];	/* 同上；PKT_MATCH_PEER 也用它 */
	pkt_u32	sport;		/* 网络序 */
	pkt_u32	dport;		/* 网络序；PEER_PORT 也用它 */
	pkt_u32	reserved[4];
};

struct net_capture_info {
	pkt_u32	max_records;	/* 输入：本次最多取多少条，0 = 按缓冲上限 */
	pkt_u32	got_records;	/* 输出 */
	pkt_u32	got_bytes;	/* 输出 */
	pkt_u32	dropped;	/* 输出：上次读取后溢出丢弃数 */
	/*
	 * 记录数组要求 8 字节对齐（pkt_record 以 u64 ts_ns 开头），
	 * 但 net_capture 在 request_obj 里的偏移不保证 8 字节对齐。
	 * 给 buf 加 aligned(8) 会把整个结构体抬到 8 字节对齐，
	 * 用户态 reinterpret_cast 成 pkt_record* 后访问 ts_ns 才是对齐的。
	 */
	pkt_u8	buf[PKT_USER_BUF_SIZE] __attribute__((aligned(8)));
};

#endif /* PKT_IO_H */