#include "asm/patching.h"
#include <linux/kprobes.h>

#define RET_INSTR 0xD65F03C0//将cfi函数的第一条命令直接修改为ret指令，直接返回，绕过cfi检查
static bool is_cfi_bypass = false;
static int __kprobes (*x_aarch64_insn_patch_text_nosync)(void *addr, u32 insn) = NULL;
static unsigned long (*kallsyms_lookup_name_fun_)(const char *name) = NULL;
static struct kprobe kp_kallsyms = {
	/* data */
	.symbol_name = "kallsyms_lookup_name"
};
__attribute__((no_sanitize("cfi"))) unsigned long util_find_kallsyms(void)
{
	int ret = -1;
	unsigned long addr = 0;

	if (kallsyms_lookup_name_fun_)
		return (uint64_t)kallsyms_lookup_name_fun_;

	ret = register_kprobe(&kp_kallsyms);

	if (ret < 0) {
		return 0;
	}
	addr = (unsigned long)kp_kallsyms.addr;
	kallsyms_lookup_name_fun_ = (void *)addr;
	unregister_kprobe(&kp_kallsyms);
	return addr;
}

__attribute__((no_sanitize("cfi"))) unsigned long util_kallsyms_lookup_name(const char *name)
{
    if (!kallsyms_lookup_name_fun_) {
        register_kprobe(&kp_kallsyms);
        kallsyms_lookup_name_fun_ = (void *)kp_kallsyms.addr;
        unregister_kprobe(&kp_kallsyms);
    }
    
    if (kallsyms_lookup_name_fun_)
        return kallsyms_lookup_name_fun_(name);
    return 0;
}

__attribute__((no_sanitize("cfi"))) void bypass_cfi(void)
{
	uint64_t cfi_addr;
	uint64_t orginal_pte;
	uint64_t *pte;
	if (is_cfi_bypass)
		return;
    x_aarch64_insn_patch_text_nosync = (void *)util_kallsyms_lookup_name("aarch64_insn_patch_text_nosync");
    if (x_aarch64_insn_patch_text_nosync == NULL)
        return;
    util_kallsyms_lookup_name("aarch64_insn_text_patch");
	cfi_addr = util_kallsyms_lookup_name("__cfi_slowpath"); //5.10
	if (cfi_addr == 0) {
		cfi_addr =
			util_kallsyms_lookup_name("__cfi_slowpath_diag"); //5.15
		if (cfi_addr == 0) {
			cfi_addr = util_kallsyms_lookup_name(
				"_cfi_slowpath"); //5.4
			if (cfi_addr == 0)
				return;
		}
	}
    x_aarch64_insn_patch_text_nosync((void *)cfi_addr, RET_INSTR);
/* 	// printk(KERN_INFO"[db]cfi_addr %llx\n", cfi_addr);
	pte = pgtable_entry_kernel(cfi_addr);
	orginal_pte = *pte;
	*pte = (orginal_pte | PTE_DBM) & ~PTE_RDONLY;
	flush_tlb_all();
	*(uint32_t *)cfi_addr = RET_INSTR;
	flush_icache_range(cfi_addr, cfi_addr + 4);
	*pte = orginal_pte;
	flush_tlb_all(); */
	is_cfi_bypass = true;
}
