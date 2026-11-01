#ifndef PKT_WORK_H
#define PKT_WORK_H

#include <linux/ptrace.h>

int pkt_ip_rcv_work(struct pt_regs *regs);
int pkt_ip6_rcv_work(struct pt_regs *regs);

int pkt_tcp_v4_rcv_work(struct pt_regs *regs);
int pkt_udp_v4_rcv_work(struct pt_regs *regs);
int pkt_icmp_v4_rcv_work(struct pt_regs *regs);

int pkt_tcp_v6_rcv_work(struct pt_regs *regs);
int pkt_udp_v6_rcv_work(struct pt_regs *regs);
int pkt_icmp_v6_rcv_work(struct pt_regs *regs);

int pkt_ip_local_out_work(struct pt_regs *regs);
int pkt_ip6_local_out_work(struct pt_regs *regs);

#endif /* PKT_WORK_H */