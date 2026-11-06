// SPDX-License-Identifier: GPL-2.0
/*
 * inline hook 的 work 函数：
 *   return 0 = 继续执行原函数
 *   return 1 = 跳过原函数并直接返回调用方
 */
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/in6.h>

#include "pkt_hook.h"
#include "pkt_filter.h"
#include "pkt_capture.h"
#include "pkt_work.h"

static int pkt_process(struct pt_regs *regs, int skb_arg, __u8 dir,
		       unsigned long drop_ret)
{
	struct sk_buff *skb;
	struct pkt_info pi;
	struct pkt_match m;

	if (!regs || skb_arg < 0 || skb_arg > 7)
		return 0;

	skb = (struct sk_buff *)(unsigned long)regs->regs[skb_arg];
	if (!skb)
		return 0;

	/* 没有启用过滤：直接放行，不解析，零开销 */
	if (!pkt_filter_active())
		return 0;

	if (pkt_inspect(skb, dir, &pi) < 0)
		return 0;

	if (pkt_trace_all)
		pkt_trace(&pi, dir == PKT_DIR_IN ? "in" : "out");

	pkt_filter_eval(&pi, &m);
	if (!m.matched)
		return 0;

	if (m.capture)
		pkt_capture_push(skb, &pi);

	if (!m.drop)
		return 0;

	/*
	 * 跳过原函数：原函数不会消费 skb，必须自己释放。
	 * 不能 kfree_skb() 之后返回 0，否则原函数会二次处理已释放的 skb。
	 */
	kfree_skb(skb);

	/*
	 * 返回 1 时跳板会恢复 regs[0..30] 再 ret，
	 * 调用方看到的返回值是这里写回 x0 的值。
	 */
	regs->regs[0] = drop_ret;
	return 1;
}

/* ---------------- 入站整包入口 ---------------- */

int pkt_ip_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);	/* 0 = NF_DROP */
}

int pkt_ip6_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

/* ---------------- 入站 L4 ---------------- */

int pkt_tcp_v4_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

int pkt_udp_v4_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

int pkt_icmp_v4_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

int pkt_tcp_v6_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

int pkt_udp_v6_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

int pkt_icmp_v6_rcv_work(struct pt_regs *regs)
{
	return pkt_process(regs, 0, PKT_DIR_IN, 0);
}

/* ---------------- 出站（可选） ---------------- */

/* ip_local_out(net, sk, skb) / ip6_local_out(net, sk, skb)，skb 是 x2 */
int pkt_ip_local_out_work(struct pt_regs *regs)
{
	return pkt_process(regs, 2, PKT_DIR_OUT,
			   (unsigned long)(long)-EPERM);
}

int pkt_ip6_local_out_work(struct pt_regs *regs)
{
	return pkt_process(regs, 2, PKT_DIR_OUT,
			   (unsigned long)(long)-EPERM);
}