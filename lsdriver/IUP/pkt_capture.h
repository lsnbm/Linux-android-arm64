#ifndef PKT_CAPTURE_H
#define PKT_CAPTURE_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include "pkt_hook.h"
#include "pkt_io.h"

int  pkt_capture_init(void);
void pkt_capture_push(struct sk_buff *skb, const struct pkt_info *pi);
int  pkt_capture_get(struct net_capture_info *info);
void pkt_capture_reset(void);

#endif /* PKT_CAPTURE_H */