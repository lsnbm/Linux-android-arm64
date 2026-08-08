# bp — ARM64 断点驱动

`bp` 是一个运行在 Android ARM64 内核中的断点驱动，提供三类断点能力，并通过稳定的
字符设备 ioctl ABI（`/dev/bp`）对外提供服务：

- **PTEBP** — PTE UXN 执行断点：将目标页标记为不可执行，命中后在**内核态批量指令模拟**
  执行，并在每次精确命中时把 S0（Q0 低 32 位）改写为配置值；Q0 高 96 位与其余寄存器保持不变。
- **HWBP** — 硬件断点/观察点：X（执行）/R/W/RW（读写观察），1–8 字节，支持逐寄存器
  2-bit 掩码（不操作 / 读快照 / 写回），以及按 PC 聚合的命中记录。
- **STEPBP** — 单步断点：硬件单步 + 代次（generation）跟踪。

## 构建

```bash
make -C <KDIR> M=$PWD/bp/kernel ARCH=arm64 LLVM=1 modules
```

- 支持 GKI 5.10 / 5.15 / 6.1 / 6.6 / 6.12 共 7 个内核版本（DDK 环境用
  `ddk build -t <target>` 亦可）。
- 模块 `bp.ko`，设备 `/dev/bp`（misc，0600），ABI 版本 1.0。
- 静态门禁：无 `R_AARCH64_AUTH` 重定位、提供 `__cfi_check`、vermagic 匹配目标内核。

## ABI

- 权威头文件：`include/bp_uapi.h`（内核镜像：`kernel/abi/bp_uapi.h`，逐字节一致）。
- 11 个 ioctl：INFO / PTE、HWBP、STEP 的 arm/status/stop / HWBP record。
- 会话模型：单一活动配置（一个 tgid）；ARM 返回 `mm_cookie`，STATUS/STOP 必须携带；
  tgid + 进程启动时刻双重校验（PID 复用拒绝）；目标 exit/exec 自动清理；模块可完全卸载。
- 详见 `docs/BREAKPOINT_ABI.md`。

## 目录结构

```
bp/
├── include/bp_uapi.h        # 用户态 ABI（权威）
├── kernel/                  # 内核驱动源码
│   ├── bp_main.c            # misc 设备 / 会话 / ioctl 分发 / 生命周期
│   ├── bp_cfi.c             # __cfi_check 桩
│   ├── bp_types.h           # 内核内部断点类型
│   ├── arm64_hwdbg.h        # 硬件断点/观察点
│   ├── arm64_ptedbg.h       # PTE 执行断点（UXN + 批量模拟）
│   ├── arm64_stepdbg.h      # 单步断点
│   ├── break_point.h        # 三族整合与命中回调
│   ├── emulate/             # 指令模拟器（emulate_insn + encode/decode）
│   ├── hook/ reg/ syms/     # inline hook 框架 / 寄存器辅助 / 符号解析
│   └── abi/bp_uapi.h        # ABI 镜像
├── docs/BREAKPOINT_ABI.md   # ABI 文档
└── Kconfig
```

## 已知限制

- QEMU virt 上 5.10 系列的 PTE 命中已通过 `el0_ia` hook 完整支持；STEP 的单步命中在
  QEMU 上可能向用户态递送 SIGTRAP（ptrace 伪单步语义），命中计数建议在真机验证。
- mmu-notifier 映射失效（fail-closed）在 5.15+/6.2+ 启用；5.10 与 6.1 使用命中时
  PTE 匹配兜底。
