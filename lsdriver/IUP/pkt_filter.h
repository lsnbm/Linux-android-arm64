#ifndef PKT_FILTER_H
#define PKT_FILTER_H

#include <linux/types.h>
#include "pkt_hook.h"
#include "pkt_io.h"

struct pkt_match {
	bool	matched;
	bool	capture;
	bool	drop;
};

bool pkt_filter_active(void);
int  pkt_filter_eval(const struct pkt_info *pi, struct pkt_match *m);
int  pkt_filter_set(const struct net_filter_info *info);
int  pkt_filter_get(struct net_filter_info *info);
void pkt_filter_disable(void);

#endif /* PKT_FILTER_H */