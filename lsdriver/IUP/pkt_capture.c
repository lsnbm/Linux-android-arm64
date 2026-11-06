// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include "pkt_capture.h"

#define PKT_RING_COUNT	256		/* 必须是 2 的幂 */

/*
 * 每条记录独立 kmalloc，而不是一次分配 256*1680≈420KB 的连续块。
 * 大块连续内存要走页分配器的高阶(order 7)，Android 开机一段时间后容易失败；
 * 1600 字节的记录落在 kmalloc-2k 缓存里，单条分配不会失败，也避免高阶碎片问题。
 */
static struct pkt_record *g_ring[PKT_RING_COUNT];
static u32 g_head;
static u32 g_tail;
static u32 g_dropped;
static DEFINE_SPINLOCK(g_ring_lock);

int pkt_capture_init(void)
{
	int i;

	if (g_ring[0])
		return 0;

	for (i = 0; i < PKT_RING_COUNT; i++) {
		g_ring[i] = kmalloc(sizeof(struct pkt_record), GFP_KERNEL);
		if (!g_ring[i]) {
			while (i-- > 0) {
				kfree(g_ring[i]);
				g_ring[i] = NULL;
			}
			return -ENOMEM;
		}
		memset(g_ring[i], 0, sizeof(struct pkt_record));
	}

	return 0;
}

void pkt_capture_push(struct sk_buff *skb, const struct pkt_info *pi)
{
	struct pkt_record *rec;
	unsigned long flags;
	int avail, copy;

	if (!g_ring[0] || !skb || !pi)
		return;

	/* 负偏移必须在 headroom 内，否则 skb_copy_bits 会读越界 */
	if (pi->l3_off < -(int)skb_headroom(skb))
		return;

	avail = (int)skb->len - pi->l3_off;
	if (avail <= 0)
		return;

	copy = min_t(int, PKT_REC_SNAPLEN, avail);

	spin_lock_irqsave(&g_ring_lock, flags);

	if (((g_head + 1) & (PKT_RING_COUNT - 1)) == g_tail) {
		g_tail = (g_tail + 1) & (PKT_RING_COUNT - 1);
		g_dropped++;
	}

	rec = g_ring[g_head];
	memset(rec, 0, sizeof(*rec));

	rec->ts_ns     = pi->ts_ns;
	rec->saddr     = pi->tu.saddr;
	rec->daddr     = pi->tu.daddr;
	memcpy(rec->saddr6, &pi->tu.saddr6, 16);
	memcpy(rec->daddr6, &pi->tu.daddr6, 16);
	rec->sport     = pi->tu.sport;
	rec->dport     = pi->tu.dport;
	rec->proto     = pi->tu.l4proto;
	rec->is_v6     = pi->tu.is_v6;
	rec->dir       = pi->dir;
	rec->tcp_flags = pi->tcp_flags;
	rec->icmp_type = pi->icmp_type;
	rec->icmp_code = pi->icmp_code;
	rec->frag      = pi->frag;
	rec->pkt_len   = pi->pkt_len;
	rec->ifindex   = (u32)pi->ifindex;

	if (copy > 0 &&
	    skb_copy_bits(skb, pi->l3_off, rec->data, copy) == 0)
		rec->cap_len = (u32)copy;

	g_head = (g_head + 1) & (PKT_RING_COUNT - 1);
	spin_unlock_irqrestore(&g_ring_lock, flags);
}

/* 清空环形缓冲：换规则、LS 退出时调用，避免旧记录残留，我在测试时就遇到过 */
void pkt_capture_reset(void)
{
	unsigned long flags;

	if (!g_ring[0])
		return;

	spin_lock_irqsave(&g_ring_lock, flags);
	g_head = 0;
	g_tail = 0;
	g_dropped = 0;
	spin_unlock_irqrestore(&g_ring_lock, flags);
}

int pkt_capture_get(struct net_capture_info *info)
{
	unsigned long flags;
	u32 max, cnt, n, i;

	if (!info || !g_ring[0])
		return -EINVAL;

	max = info->max_records;
	if (max == 0 || max > PKT_USER_BUF_SIZE / sizeof(struct pkt_record))
		max = PKT_USER_BUF_SIZE / sizeof(struct pkt_record);

	spin_lock_irqsave(&g_ring_lock, flags);

	cnt = (g_head - g_tail) & (PKT_RING_COUNT - 1);
	n = min(cnt, max);

	for (i = 0; i < n; i++) {
		memcpy(info->buf + i * sizeof(struct pkt_record),
		       g_ring[g_tail], sizeof(struct pkt_record));
		g_tail = (g_tail + 1) & (PKT_RING_COUNT - 1);
	}

	info->dropped = g_dropped;
	g_dropped = 0;

	spin_unlock_irqrestore(&g_ring_lock, flags);

	info->got_records = n;
	info->got_bytes   = n * sizeof(struct pkt_record);

	return 0;
}
