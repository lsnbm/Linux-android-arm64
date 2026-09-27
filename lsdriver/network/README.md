# Kernel IPv4 DNS and Ping

```c
#include "network/ipv4_ping.h"

int status = ipv4_ping("bing.com", 3000);
```

`ipv4_ping(host, timeout_ms)` accepts dotted IPv4 or an ASCII DNS hostname.
It returns 0 for a matching ICMP Echo Reply or a negative errno. The timeout
is a shared DNS and ICMP blocking-I/O budget, rounded to kernel ticks; scheduling
and allocation can add latency. Use only in sleepable process context, without
holding spinlocks. No main-driver integration is installed.

`ipv4_dns_resolve(host, &address, timeout_ms)` resolves an IPv4 address in network
byte order without sending ICMP. Literal IPv4 inputs do not contact DNS.
The output address is written only on success.

DNS uses Google Public DNS at 8.8.8.8, then 8.8.4.4, port 53. The primary gets
half the remaining budget, reserving time for the backup. Each query attempts
each server once. UDP replies with TC set are retried over TCP on the same
server. CNAME chains are limited to 8 links, including additional queries.
The first matching A record is returned. There is no cache, AAAA lookup, IDNA
conversion, DNSSEC validation or encrypted DNS. Unicode domains must first be
converted to ASCII punycode. Responses are not cryptographically authenticated.

Sockets use init_net and its routing. This does not inherit Android Private DNS,
per-app DNS or VPN selection. Google DNS must be reachable over UDP/TCP 53.
ICMP datagram socket creation also depends on ping_group_range and caller groups.

Typical errors: -EINVAL (input), -ETIMEDOUT (budget exhausted), -ENOENT
(NXDOMAIN), -ENODATA (no matching A record), -ELOOP (CNAME limit/loop),
-EBADMSG (malformed DNS), -EREMOTEIO (other DNS server errors), or socket errno.
