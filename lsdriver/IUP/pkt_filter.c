// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/in6.h>

#include "pkt_filter.h"
#include "pkt_capture.h"

struct pkt_filter_rule {
	struct rcu_head	rcu;
	bool		enabled;
	u8		action;
	u8		capture;
	u8		proto_mask;
	u8		dir_mask;
	u32		match_flags;
	bool		is_v6;
	__be32		saddr;
	__be32		daddr;
	struct in6_addr	saddr6;
	struct in6_addr	daddr6;
	__be16		sport;
	__be16		dport;
};

static struct pkt_filter_rule __rcu *g_rule;
static DEFINE_MUTEX(g_rule_lock);

static bool pkt_proto_match(u32 mask, u8 proto)
{
	u32 bit;

	if (!mask)
		return true;

	switch (proto) {
	case IPPROTO_TCP:	bit = PKT_PROTO_TCP_BIT; break;
	case IPPROTO_UDP:	bit = PKT_PROTO_UDP_BIT; break;
	case IPPROTO_ICMP:
	case IPPROTO_ICMPV6:	bit = PKT_PROTO_ICMP_BIT; break;
	default:		return false;
	}

	return !!(mask & bit);
}

static bool pkt_tuple_match(const struct pkt_filter_rule *r,
			    const struct pkt_tuple *t)
{
	if (!pkt_proto_match(r->proto_mask, t->l4proto))
		return false;

	if (r->match_flags & (PKT_MATCH_SADDR | PKT_MATCH_DADDR |
			      PKT_MATCH_PEER)) {
		if (r->is_v6 != !!t->is_v6)
			return false;
	}

	if (r->match_flags & PKT_MATCH_SADDR) {
		if (t->is_v6) {
			if (memcmp(&r->saddr6, &t->saddr6, 16))
				return false;
		} else if (r->saddr != t->saddr) {
			return false;
		}
	}

	if (r->match_flags & PKT_MATCH_DADDR) {
		if (t->is_v6) {
			if (memcmp(&r->daddr6, &t->daddr6, 16))
				return false;
		} else if (r->daddr != t->daddr) {
			return false;
		}
	}

	if (r->match_flags & PKT_MATCH_PEER) {
		if (t->is_v6) {
			if (memcmp(&r->daddr6, &t->saddr6, 16) &&
			    memcmp(&r->daddr6, &t->daddr6, 16))
				return false;
		} else if (r->daddr != t->saddr &&
			   r->daddr != t->daddr) {
			return false;
		}
	}

	if (r->match_flags & PKT_MATCH_SPORT) {
		if (r->sport != t->sport)
			return false;
	}

	if (r->match_flags & PKT_MATCH_DPORT) {
		if (r->dport != t->dport)
			return false;
	}

	if (r->match_flags & PKT_MATCH_PEER_PORT) {
		if (r->dport != t->sport && r->dport != t->dport)
			return false;
	}

	return true;
}

static bool pkt_filter_match(const struct pkt_info *pi,
			     const struct pkt_filter_rule *r)
{
	if (!r->enabled)
		return false;

	if (r->dir_mask) {
		u32 bit = pi->dir == PKT_DIR_IN  ? PKT_DIR_IN_BIT :
			  pi->dir == PKT_DIR_OUT ? PKT_DIR_OUT_BIT :
						   PKT_DIR_FWD_BIT;
		if (!(r->dir_mask & bit))
			return false;
	}

	/* 外层五元组命中 */
	if (pkt_tuple_match(r, &pi->tu))
		return true;

	/* ICMP 差错报文：再按引用的内层五元组匹配 */
	if (pi->has_inner && pkt_tuple_match(r, &pi->inner))
		return true;

	return false;
}

bool pkt_filter_active(void)
{
	struct pkt_filter_rule *r;
	bool on;

	rcu_read_lock();
	r = rcu_dereference(g_rule);
	on = r && r->enabled;
	rcu_read_unlock();

	return on;
}

int pkt_filter_eval(const struct pkt_info *pi, struct pkt_match *m)
{
	struct pkt_filter_rule *r;

	if (!pi || !m)
		return -EINVAL;

	memset(m, 0, sizeof(*m));

	rcu_read_lock();
	r = rcu_dereference(g_rule);
	if (r && pkt_filter_match(pi, r)) {
		m->matched = true;
		m->capture = r->capture;
		m->drop    = r->action == PKT_ACTION_DROP;
	}
	rcu_read_unlock();

	return 0;
}

int pkt_filter_set(const struct net_filter_info *info)
{
	struct pkt_filter_rule *r, *old;

	if (!info)
		return -EINVAL;

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	r->enabled     = !!info->enable;
	r->action      = info->action ? PKT_ACTION_DROP : PKT_ACTION_PASS;
	r->capture     = !!info->capture;
	r->proto_mask  = (u8)info->proto_mask;
	r->dir_mask    = (u8)info->dir_mask;
	r->match_flags = info->match_flags;
	r->is_v6       = !!info->is_v6;

	memcpy(&r->saddr,  &info->saddr[0], sizeof(r->saddr));
	memcpy(&r->daddr,  &info->daddr[0], sizeof(r->daddr));
	memcpy(&r->saddr6, info->saddr, sizeof(r->saddr6));
	memcpy(&r->daddr6, info->daddr, sizeof(r->daddr6));
	r->sport = (__be16)info->sport;
	r->dport = (__be16)info->dport;

	mutex_lock(&g_rule_lock);
	old = rcu_access_pointer(g_rule);
	rcu_assign_pointer(g_rule, r);
	mutex_unlock(&g_rule_lock);

	if (old) {
		synchronize_rcu();
		kfree(old);
	}

	/* 新规则生效后清空旧记录，避免用户读到上一次测试的包 */
	pkt_capture_reset();

	return 0;
}

/*
 * LS 退出 / 内核线程退出时清掉规则。
 * 必须在进程上下文中调用（内部会拿 mutex）。
 * 用 kfree_rcu 而不是 synchronize_rcu，避免在退出路径上等待宽限期。
 */
void pkt_filter_disable(void)
{
	struct pkt_filter_rule *old;

	mutex_lock(&g_rule_lock);
	old = rcu_access_pointer(g_rule);
	rcu_assign_pointer(g_rule, NULL);
	mutex_unlock(&g_rule_lock);

	if (old)
		kfree_rcu(old, rcu);
}

int pkt_filter_get(struct net_filter_info *info)
{
	struct pkt_filter_rule *r;

	if (!info)
		return -EINVAL;

	memset(info, 0, sizeof(*info));

	rcu_read_lock();
	r = rcu_dereference(g_rule);
	if (r) {
		info->enable      = r->enabled;
		info->action      = r->action;
		info->capture     = r->capture;
		info->proto_mask  = r->proto_mask;
		info->dir_mask    = r->dir_mask;
		info->match_flags = r->match_flags;
		info->is_v6       = r->is_v6;
		memcpy(&info->saddr[0], &r->saddr, sizeof(r->saddr));
		memcpy(&info->daddr[0], &r->daddr, sizeof(r->daddr));
		memcpy(info->saddr, &r->saddr6, sizeof(r->saddr6));
		memcpy(info->daddr, &r->daddr6, sizeof(r->daddr6));
		info->sport = r->sport;
		info->dport = r->dport;
	}
	rcu_read_unlock();

	return 0;
}