// pkt_test - 独立用户态测试程序
//
// 前提：lsdriver.ko 已由你自己加载（不需要任何模块参数），
//       且当前没有其它名为 "LS" 的进程在跑。
//
// 它做三件事：
//   1. mmap 0x2025827000，与内核握手（进程名 LS）
//   2. request_op_net_filter_set 下发过滤规则
//   3. 轮询 request_op_net_capture_get，打印抓到的 pkt_record
//
// 用法：
//   pkt_test all     8                         抓全部，跑 8 秒
//   pkt_test observe 8 203.0.113.5 443 tcp     只观察该 IP:端口 TCP
//   pkt_test drop    8 "" 0 icmp               丢弃所有入站 ICMP
//   pkt_test drop    8 1.2.3.4 53 udp          丢弃该 IP:端口 UDP
//   pkt_test observe 60 1.2.3.4 443 udp 50     第 6 个参数 = 轮询间隔 ms（默认 50）

#include <arpa/inet.h>
#include <android/log.h>
#include <csignal>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <sys/utsname.h>
#include <thread>
#include <unistd.h>

#define LS_LOG_TAG "pkt_test"
#define LS_LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LS_LOG_TAG, __VA_ARGS__))
#define LS_LOGI_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_INFO, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))
#define LS_LOGW_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_WARN, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))
#define LS_LOGE_TAG(tag, ...) ((void)__android_log_print(ANDROID_LOG_ERROR, LS_LOG_TAG, "[" tag "] " __VA_ARGS__))

#include "driver.h"     /* 复用 Driver 的共享内存握手与 request_obj 布局 */
#include "pkt_io.h"

static uint32_t proto_bit(const char *s)
{
    if (!s || !*s)
        return 0;
    if (!strcasecmp(s, "tcp"))  return PKT_PROTO_TCP_BIT;
    if (!strcasecmp(s, "udp"))  return PKT_PROTO_UDP_BIT;
    if (!strcasecmp(s, "icmp")) return PKT_PROTO_ICMP_BIT;
    return 0;
}

static const char *proto_name(uint8_t p)
{
    switch (p) {
    case 6:  return "tcp";
    case 17: return "udp";
    case 1:  return "icmp";
    case 58: return "icmp6";
    default: return "ip";
    }
}

static const char *dir_name(uint8_t d)
{
    switch (d) {
    case 0:  return "in";
    case 1:  return "out";
    case 2:  return "fwd";
    default: return "?";
    }
}

static void tcp_flag_str(uint8_t f, char *out, size_t n)
{
    static const char *names[] = { "FIN", "SYN", "RST", "PSH", "ACK", "URG" };
    size_t pos = 0;

    out[0] = '\0';
    for (int i = 0; i < 6; i++) {
        if (!(f & (1u << i)))
            continue;
        int w = snprintf(out + pos, pos < n ? n - pos : 0, "%s%s",
                         pos ? "," : "", names[i]);
        if (w > 0)
            pos += (size_t)w;
    }
    if (!pos)
        snprintf(out, n, "none");
}

/* 从 IP 头起始处 dump 全部快照字节：偏移 + 十六进制 + ASCII */
static void dump_bytes(const uint8_t *p, uint32_t n)
{
    for (uint32_t off = 0; off < n; off += 16) {
        uint32_t line = (n - off) < 16 ? (n - off) : 16;

        printf("      %04x:", off);
        for (uint32_t i = 0; i < 16; i++) {
            if (i < line)
                printf(" %02x", p[off + i]);
            else
                printf("   ");
        }
        printf("  |");
        for (uint32_t i = 0; i < line; i++) {
            uint8_t c = p[off + i];
            putchar(c >= 0x20 && c < 0x7f ? c : '.');
        }
        printf("|\n");
    }
}

static void print_record(const struct pkt_record *r)
{
    char sa[64], da[64];
    char flags[32];

    if (r->is_v6) {
        inet_ntop(AF_INET6, r->saddr6, sa, sizeof(sa));
        inet_ntop(AF_INET6, r->daddr6, da, sizeof(da));
    } else {
        struct in_addr a, b;
        a.s_addr = r->saddr;
        b.s_addr = r->daddr;
        inet_ntop(AF_INET, &a, sa, sizeof(sa));
        inet_ntop(AF_INET, &b, da, sizeof(da));
    }

    printf("[rec] %-5s %s %s:%u -> %s:%u len=%u cap=%u dir=%s if=%u\n",
           proto_name(r->proto), r->is_v6 ? "v6" : "v4",
           sa, ntohs(r->sport), da, ntohs(r->dport),
           r->pkt_len, r->cap_len, dir_name(r->dir), r->ifindex);

    printf("      ts=%llu.%09llu frag=0x%02x",
           (unsigned long long)(r->ts_ns / 1000000000ULL),
           (unsigned long long)(r->ts_ns % 1000000000ULL), r->frag);
    if (r->proto == 6) {
        tcp_flag_str(r->tcp_flags, flags, sizeof(flags));
        printf(" tcp_flags=0x%02x(%s)", r->tcp_flags, flags);
    } else if (r->proto == 1 || r->proto == 58) {
        printf(" icmp_type=%u icmp_code=%u", r->icmp_type, r->icmp_code);
    }
    printf("\n");

    if (r->cap_len)
        dump_bytes(r->data, r->cap_len);

    fflush(stdout);
}

static void on_alarm(int)
{
    static const char msg[] =
        "TIMEOUT: 内核 20 秒未响应。\n"
        "可能 1) 加载的是旧版驱动（没有 request_op_net_*）；\n"
        "     2) 旧驱动已经把 dispatch 线程当成 kernel_exit 退出了。\n"
        "请重启手机，然后加载副本里新编的 androidXX-...lsdriver.ko。\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(9);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "all";
    int duration     = argc > 2 ? atoi(argv[2]) : 8;
    const char *ip   = argc > 3 ? argv[3] : "";
    int port         = argc > 4 ? atoi(argv[4]) : 0;
    const char *proto = argc > 5 ? argv[5] : "";
    int poll_ms      = argc > 6 ? atoi(argv[6]) : 50;

    if (duration <= 0)
        duration = 8;
    if (poll_ms < 1)
        poll_ms = 50;

    setvbuf(stdout, nullptr, _IOLBF, 0);

    struct utsname uts {};
    if (uname(&uts) == 0)
        printf("kernel=%s pid=%d\n", uts.release, getpid());

    signal(SIGALRM, on_alarm);
    alarm((unsigned)(duration + 20));

    try {
        Driver drv(0, false, false);

        bool supported = false;
        int pst = drv.ProbeNetOps(supported);
        printf("ProbeNetOps status=%d supported=%d\n", pst, (int)supported);
        fflush(stdout);
        if (!supported) {
            fprintf(stderr,
                    "驱动不支持 request_op_net_*：你加载的是旧版 lsdriver.ko。\n"
                    "请重启后加载副本里新编的 androidXX-...lsdriver.ko。\n");
            return 4;
        }

        struct net_filter_info f {};
        f.enable  = 1;
        f.capture = 1;
        f.action  = !strcmp(mode, "drop") ? PKT_ACTION_DROP : PKT_ACTION_PASS;
        f.proto_mask = proto_bit(proto);

        if (ip && *ip) {
            struct in_addr a4 {};
            struct in6_addr a6 {};

            if (inet_pton(AF_INET, ip, &a4) == 1) {
                f.match_flags |= PKT_MATCH_PEER;
                f.daddr[0] = a4.s_addr;
            } else if (inet_pton(AF_INET6, ip, &a6) == 1) {
                f.is_v6 = 1;
                f.match_flags |= PKT_MATCH_PEER;
                memcpy(f.daddr, &a6, sizeof(a6));
            } else {
                fprintf(stderr, "bad ip: %s\n", ip);
                return 2;
            }

            if (port > 0) {
                f.match_flags |= PKT_MATCH_PEER_PORT;
                f.dport = htons((uint16_t)port);
            }
        }

        int st = drv.SetNetFilter(f);
        printf("SetNetFilter=%d\n", st);
        if (st != 0)
            return 3;

        struct net_filter_info back {};
        drv.GetNetFilter(back);
        printf("filter: enable=%u action=%u capture=%u proto=0x%x match=0x%x v6=%u port=%u\n",
               back.enable, back.action, back.capture, back.proto_mask,
               back.match_flags, back.is_v6, ntohs((uint16_t)back.dport));
        printf("poll=%dms\n", poll_ms);
        fflush(stdout);

        auto end = std::chrono::steady_clock::now() +
                   std::chrono::seconds(duration);

        unsigned long long total_rec = 0;
        unsigned long long total_drop = 0;

        while (std::chrono::steady_clock::now() < end) {
            struct net_capture_info cap {};
            int r = drv.GetCapturedPackets(cap);

            if (r == 0) {
                const struct pkt_record *rec =
                    reinterpret_cast<const struct pkt_record *>(cap.buf);
                for (uint32_t i = 0; i < cap.got_records; i++)
                    print_record(&rec[i]);
                total_rec += cap.got_records;
                total_drop += cap.dropped;
                if (cap.dropped)
                    printf("ring_dropped=%u (total=%llu)\n",
                           cap.dropped, total_drop);
                fflush(stdout);
            } else {
                fprintf(stderr, "GetCapturedPackets=%d\n", r);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
        }

        drv.DisableNetFilter();
        alarm(0);
        printf("stats: records=%llu ring_dropped=%llu poll=%dms\n",
               total_rec, total_drop, poll_ms);
        printf("done\n");
    } catch (const std::exception &e) {
        fprintf(stderr, "error: %s\n", e.what());
        fprintf(stderr,
                "握手失败 = 内核没有在 5 秒内绑定名为 \"LS\" 的测试进程。请查：\n"
                "  1) 上次旧驱动执行过 kernel_exit 后线程已退出，必须重启手机；\n"
                "  2) 新 ko 是否真的 insmod 成功（version magic / 签名 / 依赖）；\n"
                "  3) 是否还有别的 LS 进程（先退出 LS 主程序）。\n"
                "排查：\n"
                "  uname -r\n"
                "  dmesg | grep -iE 'lsdriver|version magic|disagrees' | tail -30\n"
                "  ps -A -o PID,PPID,NAME | grep -E 'LS|pkt_test'\n");
        return 1;
    }

    return 0;
}
