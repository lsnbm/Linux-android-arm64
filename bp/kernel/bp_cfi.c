/* SPDX-License-Identifier: GPL-2.0 */
/*
 * __cfi_check stub for GKI module load. Compile with -fno-sanitize=cfi.
 */
#include <linux/types.h>

void bp_cfi_check(u64 id, void *ptr, void *diag)
{
	(void)id;
	(void)ptr;
	(void)diag;
}

extern void __cfi_check(u64 id, void *ptr, void *diag)
	__attribute__((alias("bp_cfi_check")));
