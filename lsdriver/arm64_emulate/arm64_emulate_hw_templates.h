#ifndef ARM64_EMULATE_HW_TEMPLATES_H
#define ARM64_EMULATE_HW_TEMPLATES_H

#include <linux/types.h>

#define ARM64_HW_TEMPLATE static __attribute__((__naked__, __noinline__, __unused__, __section__(".text.arm64_hw_templates")))

/*
快速解释硬件汇编模板执行模型：

1. executor leaf 根据原机器码中的 Rd/Rn/Rm/Ra，从 pt_regs 或 fp_regs 读取
	对应的软件寄存器现场。原指令的寄存器编号只用于选择软件现场槽位，不直接
	决定模板使用的硬件寄存器编号。

2. leaf 按 AAPCS64 将值、地址和输出指针放入 x0-x7，再调用一条具体模板。
	通用运算固定借用 x0-x7；FP/Advanced SIMD 运算通常由 x0-x3 传递
	fp_regs->q[] 地址，并固定借用 v0-v3 完成装载、运算和保存。因此不需要为
	原指令所有 Rd/Rn/Rm/Ra 组合生成模板，只保留操作、数据形态和编码立即数
	等真正影响机器码的变体。

3. 模板不会自动把被修改的参数寄存器同步到软件现场。通用结果通过 x0 返回，
	或通过输出指针保存，再由 leaf 使用 reg_write()、addr_reg_write()、
	emu_write_nzcv() 等显式提交到 pt_regs。FP/Advanced SIMD 模板通常把结果
	直接保存到 x0 指向的 fp_regs->q[Rd]。

4. 外层异常处理在一批模拟开始前将真实 Q0-Q31、FPCR 和 FPSR 快照到 fp_regs，
	批处理结束后再把完整软件现场写回 CPU。模板可以把 v0-v3 当作临时寄存器，
	不依赖调用前这些硬件寄存器中保存的架构值。

5. load/store、原子、屏障等需要真实硬件副作用的指令同样使用固定参数寄存器
	模板。分支、PC 更新以及不适合在模板地址直接执行的系统语义由 executor leaf
	修改软件现场，不强行执行原始硬件指令。编码中的 immediate、lane、rotation
	等不能由参数寄存器替换的字段仍对应独立的固定模板。

模板函数必须保持 naked、noinline，只允许包含 basic asm，并由模板显式 ret；
禁止在模板函数体内声明局部变量、访问 C 参数或加入任何 C 控制流。

Makefile 全局启用模板显式汇编需要的 ISA 扩展，并禁止普通 C 自动向量化；
Clang 12 无法识别的助记符继续使用 .inst。编译配置不提供运行时 CPU 特性检查。

说一下后续新指令添加到文件，
使用两级注释分隔模板：
带“========================”的标题对应五大执行器类别，
带“----------”的标题对应大类内部的具体语义子组。
同一类别、同一语义的模板应连续放置；
新增模板必须归入对应分组，避免不同执行器类别或无关指令族相互穿插。
*/

// clang-format off

/* ======================== 分支、异常与系统指令模板 ======================== */

/* ---------- CLREX ---------- */

ARM64_HW_TEMPLATE void emu_template_clrex_option_0(void)
{ asm volatile("clrex #0\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_1(void)
{ asm volatile("clrex #1\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_2(void)
{ asm volatile("clrex #2\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_3(void)
{ asm volatile("clrex #3\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_4(void)
{ asm volatile("clrex #4\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_5(void)
{ asm volatile("clrex #5\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_6(void)
{ asm volatile("clrex #6\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_7(void)
{ asm volatile("clrex #7\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_8(void)
{ asm volatile("clrex #8\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_9(void)
{ asm volatile("clrex #9\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_10(void)
{ asm volatile("clrex #10\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_11(void)
{ asm volatile("clrex #11\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_12(void)
{ asm volatile("clrex #12\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_13(void)
{ asm volatile("clrex #13\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_14(void)
{ asm volatile("clrex #14\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_clrex_option_15(void)
{ asm volatile("clrex #15\nret\n"); }

/* ---------- DSB ---------- */

ARM64_HW_TEMPLATE void emu_template_dsb_option_0(void)
{ asm volatile("dsb #0\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_1(void)
{ asm volatile("dsb #1\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_2(void)
{ asm volatile("dsb #2\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_3(void)
{ asm volatile("dsb #3\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_4(void)
{ asm volatile("dsb #4\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_5(void)
{ asm volatile("dsb #5\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_6(void)
{ asm volatile("dsb #6\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_7(void)
{ asm volatile("dsb #7\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_8(void)
{ asm volatile("dsb #8\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_9(void)
{ asm volatile("dsb #9\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_10(void)
{ asm volatile("dsb #10\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_11(void)
{ asm volatile("dsb #11\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_12(void)
{ asm volatile("dsb #12\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_13(void)
{ asm volatile("dsb #13\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_14(void)
{ asm volatile("dsb #14\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dsb_option_15(void)
{ asm volatile("dsb #15\nret\n"); }

/* ---------- DMB ---------- */

ARM64_HW_TEMPLATE void emu_template_dmb_option_0(void)
{ asm volatile("dmb #0\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_1(void)
{ asm volatile("dmb #1\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_2(void)
{ asm volatile("dmb #2\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_3(void)
{ asm volatile("dmb #3\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_4(void)
{ asm volatile("dmb #4\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_5(void)
{ asm volatile("dmb #5\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_6(void)
{ asm volatile("dmb #6\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_7(void)
{ asm volatile("dmb #7\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_8(void)
{ asm volatile("dmb #8\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_9(void)
{ asm volatile("dmb #9\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_10(void)
{ asm volatile("dmb #10\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_11(void)
{ asm volatile("dmb #11\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_12(void)
{ asm volatile("dmb #12\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_13(void)
{ asm volatile("dmb #13\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_14(void)
{ asm volatile("dmb #14\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_dmb_option_15(void)
{ asm volatile("dmb #15\nret\n"); }

/* ---------- ISB ---------- */

ARM64_HW_TEMPLATE void emu_template_isb(void)
{ asm volatile("isb\nret\n"); }

/* ---------- YIELD ---------- */

ARM64_HW_TEMPLATE void emu_template_yield(void)
{ asm volatile("yield\nret\n"); }

/* ======================== 访存指令模板 ======================== */

/* ---------- GPR load ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_ldrb_w(uint64_t addr)
{ asm volatile("ldrb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrh_w(uint64_t addr)
{ asm volatile("ldrh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldr_w(uint64_t addr)
{ asm volatile("ldr w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldr_x(uint64_t addr)
{ asm volatile("ldr x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrsb_w(uint64_t addr)
{ asm volatile("ldrsb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrsb_x(uint64_t addr)
{ asm volatile("ldrsb x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrsh_w(uint64_t addr)
{ asm volatile("ldrsh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrsh_x(uint64_t addr)
{ asm volatile("ldrsh x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldrsw_x(uint64_t addr)
{ asm volatile("ldrsw x0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldurb_w(uint64_t addr)
{ asm volatile("ldurb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldurh_w(uint64_t addr)
{ asm volatile("ldurh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldur_w(uint64_t addr)
{ asm volatile("ldur w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldur_x(uint64_t addr)
{ asm volatile("ldur x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldursb_w(uint64_t addr)
{ asm volatile("ldursb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldursb_x(uint64_t addr)
{ asm volatile("ldursb x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldursh_w(uint64_t addr)
{ asm volatile("ldursh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldursh_x(uint64_t addr)
{ asm volatile("ldursh x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldursw_x(uint64_t addr)
{ asm volatile("ldursw x0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldtrb_w(uint64_t addr)
{ asm volatile("ldtrb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrh_w(uint64_t addr)
{ asm volatile("ldtrh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtr_w(uint64_t addr)
{ asm volatile("ldtr w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtr_x(uint64_t addr)
{ asm volatile("ldtr x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrsb_w(uint64_t addr)
{ asm volatile("ldtrsb w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrsb_x(uint64_t addr)
{ asm volatile("ldtrsb x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrsh_w(uint64_t addr)
{ asm volatile("ldtrsh w0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrsh_x(uint64_t addr)
{ asm volatile("ldtrsh x0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldtrsw_x(uint64_t addr)
{ asm volatile("ldtrsw x0, [x0]\nret\n"); }

/* ---------- GPR store ---------- */

ARM64_HW_TEMPLATE void emu_template_sturb_w(uint64_t addr, uint64_t value)
{ asm volatile("sturb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_sturh_w(uint64_t addr, uint64_t value)
{ asm volatile("sturh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_w(uint64_t addr, uint64_t value)
{ asm volatile("stur w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_x(uint64_t addr, uint64_t value)
{ asm volatile("stur x1, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_sttrb_w(uint64_t addr, uint64_t value)
{ asm volatile("sttrb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_sttrh_w(uint64_t addr, uint64_t value)
{ asm volatile("sttrh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_sttr_w(uint64_t addr, uint64_t value)
{ asm volatile("sttr w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_sttr_x(uint64_t addr, uint64_t value)
{ asm volatile("sttr x1, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_strb_w(uint64_t addr, uint64_t value)
{ asm volatile("strb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_strh_w(uint64_t addr, uint64_t value)
{ asm volatile("strh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_w(uint64_t addr, uint64_t value)
{ asm volatile("str w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_x(uint64_t addr, uint64_t value)
{ asm volatile("str x1, [x0]\nret\n"); }

/* ---------- RCpc ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_ldapurb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapurb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapurh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapurh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapur_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapur w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapur_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldapur x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapursb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapursb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapursb_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldapursb x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapursh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapursh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapursh_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldapursh x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapursw_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldapursw x0, [x1]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_stlurb_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlurb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlurh_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlurh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlur_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlur w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlur_x(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlur x0, [x1]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldaprb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldaprb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaprh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldaprh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapr_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldapr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldapr_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldapr x0, [x1]\nret\n"); }

/* ---------- ordered ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_ldlarb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldlarb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldlarh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldlarh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldlar_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldlar w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldlar_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldlar x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldarb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldarb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldarh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldarh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldar_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldar w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldar_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldar x0, [x1]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_stllrb_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stllrh_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stllr_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stllr_x(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllr x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlrb_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlrh_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlr_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stlr_x(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlr x0, [x1]\nret\n"); }

/* ---------- exclusive ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_ldxrb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldxrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaxrb_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldaxrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldxrh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldxrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaxrh_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldaxrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldxr_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldxr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaxr_w(uint64_t addr)
{ asm volatile("mov x1, x0\nldaxr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldxr_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldxr x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaxr_x(uint64_t addr)
{ asm volatile("mov x1, x0\nldaxr x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldxp_w(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldxp w3, w4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldaxp_w(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldaxp w3, w4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldxp_x(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldxp x3, x4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldaxp_x(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldaxp x3, x4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint32_t emu_template_stxrb_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstxrb w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxrb_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstlxrb w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stxrh_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstxrh w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxrh_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstlxrh w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stxr_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstxr w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxr_w(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstlxr w0, w1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stxr_x(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstxr w0, x1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxr_x(uint64_t addr, uint64_t value)
{ asm volatile("mov x2, x0\nstlxr w0, x1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stxp_w(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("mov x3, x0\nstxp w0, w1, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxp_w(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("mov x3, x0\nstlxp w0, w1, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stxp_x(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("mov x3, x0\nstxp w0, x1, x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_stlxp_x(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("mov x3, x0\nstlxp w0, x1, x2, [x3]\nret\n"); }

/* ---------- LSE RMW ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_ldadd_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadd_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadd_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldadd w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadd_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldadd x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadda_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadda_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadda_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldadda w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldadda_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldadda x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldaddal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldaddal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldclr_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclr_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclr_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclr w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclr_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclr x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclra_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclra_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclra_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclra w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclra_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclra x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclrl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclrl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclrl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclrl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclrl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclral_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclralb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclral_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclralh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclral_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclral w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldclral_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldclral x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldeor_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeor_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeor_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeor w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeor_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeor x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeora_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeora_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeora_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeora w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeora_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeora x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeorl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeorl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeorl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeorl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeorl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeoral_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeoralb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeoral_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeoralh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeoral_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeoral w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldeoral_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldeoral x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldset_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldset_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldseth w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldset_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldset w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldset_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldset x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldseta_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldseta_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldseta_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldseta w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldseta_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldseta x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsetal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsetal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldsmax_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmax_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmax_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmax w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmax_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmax x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxa_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxa_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxa_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxa w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxa_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxa x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmaxal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmaxal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldsmin_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmin_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmin_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmin w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmin_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmin x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmina_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmina_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmina_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmina w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsmina_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsmina x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldsminal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldsminal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldumax_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumax_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumax_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumax w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumax_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumax x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxa_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxa_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxa_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxa w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxa_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxa x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumaxal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumaxal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_ldumin_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumin_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumin_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumin w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumin_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumin x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumina_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumina_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumina_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumina w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ldumina_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nldumina x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminlb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminlh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lduminal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nlduminal x1, x0, [x2]\nret\n"); }

ARM64_HW_TEMPLATE uint64_t emu_template_swp_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swp_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswph w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swp_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswp w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swp_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswp x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpa_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpab w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpa_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpah w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpa_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpa w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpa_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpa x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpl_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswplb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpl_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswplh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpl_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpl w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpl_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpl x1, x0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpal_b(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpalb w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpal_h(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpalh w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpal_w(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpal w1, w0, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_swpal_x(uint64_t addr, uint64_t src)
{ asm volatile("mov x2, x0\nswpal x1, x0, [x2]\nret\n"); }

/* ---------- CAS ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_cas_b(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasb w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_cas_h(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncash w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_cas_w(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncas w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_cas_x(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov x0, x1\ncas x0, x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casa_b(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasab w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casa_h(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasah w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casa_w(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasa w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casa_x(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasa x0, x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casl_b(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncaslb w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casl_h(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncaslh w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casl_w(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasl w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casl_x(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasl x0, x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casal_b(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasalb w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casal_h(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasalh w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casal_w(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasal w0, w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_casal_x(uint64_t addr, uint64_t expected, uint64_t desired)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasal x0, x2, [x3]\nret\n"); }

/* ---------- CASP ---------- */

ARM64_HW_TEMPLATE void emu_template_casp_w(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncasp w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_casp_x(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncasp x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspa_w(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspa w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspa_x(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspa x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspl_w(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspl w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspl_x(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspl x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspal_w(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspal w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_caspal_x(uint64_t addr, uint64_t expected0, uint64_t expected1, uint64_t desired0, uint64_t desired1, uint64_t *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspal x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }

/* ---------- FP/SIMD load ---------- */

ARM64_HW_TEMPLATE void emu_template_ldur_fp_b(uint64_t addr, void *out)
{ asm volatile("ldur b0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldur_fp_h(uint64_t addr, void *out)
{ asm volatile("ldur h0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldur_fp_s(uint64_t addr, void *out)
{ asm volatile("ldur s0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldur_fp_d(uint64_t addr, void *out)
{ asm volatile("ldur d0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldur_fp_q(uint64_t addr, void *out)
{ asm volatile("ldur q0, [x0]\nstr q0, [x1]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_ldr_fp_b(uint64_t addr, void *out)
{ asm volatile("ldr b0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldr_fp_h(uint64_t addr, void *out)
{ asm volatile("ldr h0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldr_fp_s(uint64_t addr, void *out)
{ asm volatile("ldr s0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldr_fp_d(uint64_t addr, void *out)
{ asm volatile("ldr d0, [x0]\nstr q0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldr_fp_q(uint64_t addr, void *out)
{ asm volatile("ldr q0, [x0]\nstr q0, [x1]\nret\n"); }

/* ---------- FP/SIMD store ---------- */

ARM64_HW_TEMPLATE void emu_template_stur_fp_b(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstur b0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_fp_h(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstur h0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_fp_s(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstur s0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_fp_d(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstur d0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stur_fp_q(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstur q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_str_fp_b(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstr b0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_fp_h(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstr h0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_fp_s(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstr s0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_fp_d(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstr d0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_str_fp_q(uint64_t addr, const void *value)
{ asm volatile("ldr q0, [x1]\nstr q0, [x0]\nret\n"); }

/* ---------- GPR pair load/store ---------- */

ARM64_HW_TEMPLATE void emu_template_ldnp_gpr_w(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldnp w3, w4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldnp_gpr_x(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldnp x3, x4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldp_gpr_w(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldp w3, w4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldp_gpr_x(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldp x3, x4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldpsw_gpr_x(uint64_t addr, uint64_t *first, uint64_t *second)
{ asm volatile("ldpsw x3, x4, [x0]\nstr x3, [x1]\nstr x4, [x2]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_stnp_gpr_w(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("stnp w1, w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stnp_gpr_x(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("stnp x1, x2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stp_gpr_w(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("stp w1, w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stp_gpr_x(uint64_t addr, uint64_t first, uint64_t second)
{ asm volatile("stp x1, x2, [x0]\nret\n"); }

/* ---------- FP/SIMD pair load/store ---------- */

ARM64_HW_TEMPLATE void emu_template_ldnp_fp_s(uint64_t addr, void *first, void *second)
{ asm volatile("ldnp s0, s1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldnp_fp_d(uint64_t addr, void *first, void *second)
{ asm volatile("ldnp d0, d1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldnp_fp_q(uint64_t addr, void *first, void *second)
{ asm volatile("ldnp q0, q1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldp_fp_s(uint64_t addr, void *first, void *second)
{ asm volatile("ldp s0, s1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldp_fp_d(uint64_t addr, void *first, void *second)
{ asm volatile("ldp d0, d1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_ldp_fp_q(uint64_t addr, void *first, void *second)
{ asm volatile("ldp q0, q1, [x0]\nstr q0, [x1]\nstr q1, [x2]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_stnp_fp_s(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp s0, s1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stnp_fp_d(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp d0, d1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stnp_fp_q(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp q0, q1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stp_fp_s(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp s0, s1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stp_fp_d(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp d0, d1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_stp_fp_q(uint64_t addr, const void *first, const void *second)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp q0, q1, [x0]\nret\n"); }

/* ======================== FP 与 Advanced SIMD 指令模板 ======================== */

/* ---------- FP/AdvSIMD merge conversion ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_fcvt_d_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvt d0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fcvt_s_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvt s0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_xtn2_16b_8h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.16b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_xtn2_8h_4s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.8h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_xtn2_4s_2d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.4s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn2_16b_8h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.16b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn2_8h_4s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.8h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn2_4s_2d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.4s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun2_16b_8h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.16b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun2_8h_4s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.8h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun2_4s_2d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.4s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn2_16b_8h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.16b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn2_8h_4s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.8h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn2_4s_2d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.4s, v1.2d\nstr q0, [x0]\nret\n"); }

/* ---------- GPR to FP ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_scvtf_s_w_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nscvtf s0, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_ucvtf_s_w_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nucvtf s0, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_scvtf_d_w_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nscvtf d0, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_ucvtf_d_w_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nucvtf d0, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_scvtf_s_x_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nscvtf s0, x1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_ucvtf_s_x_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nucvtf s0, x1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_scvtf_d_x_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nscvtf d0, x1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_ucvtf_d_x_merge(void *dst, uint64_t value)
{ asm volatile("ldr q0, [x0]\nucvtf d0, x1\nstr q0, [x0]\nret\n"); }

/* ---------- FP/AdvSIMD conversion ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fcvtns_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtns s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtns_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtns d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtns_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtns_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtns_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtms_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtms s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtms_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtms d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtms_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtms_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtms_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtas_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtas s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtas_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtas d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtas_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtas_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtas_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_scvtf_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nscvtf s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_scvtf_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nscvtf d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_scvtf_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nscvtf v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_scvtf_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nscvtf v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_scvtf_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nscvtf v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtps_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtps s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtps_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtps d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtps_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtps_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtps_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtzs_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzs s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzs_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzs d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzs_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzs_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzs_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtnu_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtnu s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtnu_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtnu d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtnu_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtnu_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtnu_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtmu_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtmu s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtmu_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtmu d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtmu_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtmu_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtmu_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtau_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtau s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtau_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtau d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtau_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtau_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtau_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_ucvtf_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nucvtf s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ucvtf_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nucvtf d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ucvtf_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nucvtf v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ucvtf_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nucvtf v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ucvtf_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nucvtf v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtpu_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtpu s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtpu_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtpu d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtpu_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtpu_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtpu_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcvtzu_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzu s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzu_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzu d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzu_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzu_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcvtzu_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

/* ---------- FP to GPR ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtns_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtns w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtns_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtns w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtns_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtns x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtns_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtns x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtnu_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtnu w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtnu_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtnu w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtnu_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtnu x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtnu_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtnu x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtas_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtas w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtas_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtas w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtas_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtas x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtas_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtas x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtau_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtau w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtau_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtau w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtau_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtau x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtau_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtau x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtps_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtps w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtps_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtps w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtps_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtps x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtps_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtps x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtpu_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtpu w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtpu_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtpu w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtpu_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtpu x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtpu_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtpu x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtms_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtms w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtms_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtms w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtms_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtms x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtms_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtms x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtmu_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtmu w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtmu_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtmu w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtmu_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtmu x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtmu_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtmu x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzs_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzs w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzs_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzs w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzs_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzs x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzs_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzs x0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzu_w_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzu w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzu_w_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzu w0, d1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzu_x_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzu x0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcvtzu_x_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcvtzu x0, d1\nret\n"); }

/* ---------- FP select ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_fcsel_h(void *dst, const void *left, const void *right, uint32_t take)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel h0, h1, h2, ne\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fcsel_s(void *dst, const void *left, const void *right, uint32_t take)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel s0, s1, s2, ne\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fcsel_d(void *dst, const void *left, const void *right, uint32_t take)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel d0, d1, d2, ne\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD lane and scalar transfer ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_b(const void *source, uint32_t lane)
{ asm volatile("ldrb w0, [x0, w1, uxtw]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_h(const void *source, uint32_t lane)
{ asm volatile("ldrh w0, [x0, w1, uxtw #1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_s(const void *source, uint32_t lane)
{ asm volatile("ldr w0, [x0, w1, uxtw #2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_d(const void *source, uint32_t lane)
{ asm volatile("ldr x0, [x0, w1, uxtw #3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_signed_b_w(const void *source, uint32_t lane)
{ asm volatile("ldrsb w0, [x0, w1, uxtw]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_signed_h_w(const void *source, uint32_t lane)
{ asm volatile("ldrsh w0, [x0, w1, uxtw #1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_signed_b_x(const void *source, uint32_t lane)
{ asm volatile("ldrsb x0, [x0, w1, uxtw]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_signed_h_x(const void *source, uint32_t lane)
{ asm volatile("ldrsh x0, [x0, w1, uxtw #1]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_extract_signed_s_x(const void *source, uint32_t lane)
{ asm volatile("ldrsw x0, [x0, w1, uxtw #2]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_insert_b(void *dst, uint64_t value, uint32_t lane)
{ asm volatile("strb w1, [x0, w2, uxtw]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_insert_h(void *dst, uint64_t value, uint32_t lane)
{ asm volatile("strh w1, [x0, w2, uxtw #1]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_insert_s(void *dst, uint64_t value, uint32_t lane)
{ asm volatile("str w1, [x0, w2, uxtw #2]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_insert_d(void *dst, uint64_t value, uint32_t lane)
{ asm volatile("str x1, [x0, w2, uxtw #3]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_write_scalar_s(void *dst, uint64_t value)
{ asm volatile("fmov s0, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_write_scalar_d(void *dst, uint64_t value)
{ asm volatile("fmov d0, x1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_read_scalar_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfmov w0, s1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_simd_read_scalar_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfmov x0, d1\nret\n"); }

/* ---------- SIMD duplicate general ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_dup_8b(void *dst, uint64_t value)
{ asm volatile("dup v0.8b, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_4h(void *dst, uint64_t value)
{ asm volatile("dup v0.4h, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_2s(void *dst, uint64_t value)
{ asm volatile("dup v0.2s, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_16b(void *dst, uint64_t value)
{ asm volatile("dup v0.16b, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_8h(void *dst, uint64_t value)
{ asm volatile("dup v0.8h, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_4s(void *dst, uint64_t value)
{ asm volatile("dup v0.4s, w1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_dup_2d(void *dst, uint64_t value)
{ asm volatile("dup v0.2d, x1\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD RDM accumulate ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_h_scalar_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlah_s_scalar_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah s0, s1, s2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_h_scalar_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmlsh_s_scalar_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh s0, s1, s2\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD permute vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_uzp1_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp1_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_trn1_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn1_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_zip1_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip1_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uzp2_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uzp2_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_trn2_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_trn2_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_zip2_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_zip2_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD integer accumulate vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_saba_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saba_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saba_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saba_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saba_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saba_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_mla_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mla_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mla_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mla_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mla_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mla_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uaba_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaba_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaba_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaba_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaba_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaba_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_mls_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mls_4h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mls_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mls_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mls_8h_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mls_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD saturating add/sub vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sqadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqsub_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqsub_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD integer compare vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_cmgt_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_cmge_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_cmtst_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_cmhi_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_cmhs_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_cmeq_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD variable shift vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sshl_8b_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.8b, w2\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_4h_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.4h, w2\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_2s_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.2s, w2\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_16b_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.16b, w2\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_8h_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.8h, w2\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_4s_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.4s, w2\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_2d_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.2d, x2\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_ushl_8b_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.8b, w2\nushl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_4h_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.4h, w2\nushl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_2s_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.2s, w2\nushl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_16b_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.16b, w2\nushl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_8h_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.8h, w2\nushl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_4s_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.4s, w2\nushl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_2d_scalar_amount(void *dst, const void *source, uint64_t shift_amount)
{ asm volatile("ldr q1, [x1]\ndup v2.2d, x2\nushl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD extract ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_0(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_1(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_2(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_3(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_4(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #4\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_5(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #5\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_6(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #6\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_8b_offset_7(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #7\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_0(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_1(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_2(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_3(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_4(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #4\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_5(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #5\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_6(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #6\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_7(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #7\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_8(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #8\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_9(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #9\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_10(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #10\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_11(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #11\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_12(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #12\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_13(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #13\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_14(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #14\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ext_16b_offset_15(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #15\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_srshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_ushl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_urshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD integer add/sub vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_add_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_addp_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sub_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD halving add/sub vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_shadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_srhadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srhadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srhadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srhadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srhadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srhadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_shsub_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shsub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shsub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shsub_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shsub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_shsub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uhadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_urhadd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urhadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urhadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urhadd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urhadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urhadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uhsub_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhsub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhsub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhsub_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhsub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uhsub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD min/max/absdiff vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_smax_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smax_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smax_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smax_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smax_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smax_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_smin_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smin_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smin_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smin_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smin_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smin_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sabd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sabd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sabd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sabd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sabd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sabd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_umax_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umax_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umax_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umax_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umax_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umax_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_umin_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umin_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umin_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umin_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umin_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umin_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uabd_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uabd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uabd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uabd_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uabd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uabd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD multiply/pairwise vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_mul_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mul_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mul_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mul_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mul_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mul_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_smaxp_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxp_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sminp_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminp_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_umaxp_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxp_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uminp_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminp_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD special multiply vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_pmul_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\npmul v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_pmul_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\npmul v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD dot/matrix accumulate ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sdot_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.2s, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sdot_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_usdot_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.2s, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_usdot_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfdot_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.2s, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfdot_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.4s, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_udot_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.2s, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_udot_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smmla_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsmmla v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_usmmla_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusmmla v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfmmla_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmmla v0.4s, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ummla_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nummla v0.4s, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD saturating scalar ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sqadd_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqadd_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqsub_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqsub_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqshl_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqadd_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqadd_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqsub_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqsub_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqshl_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_b_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl b0, b1, b2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqrshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl d0, d1, d2\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD integer scalar ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqdmulh_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmgt_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmge_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_srshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_add_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmtst_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqrdmulh_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhi_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmhs_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_ushl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_urshl_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sub_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_cmeq_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq d0, d1, d2\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP scalar ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fmulx_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_frecps_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmge_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_facge_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fabd_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt d0, d1, d2\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_facgt_h_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_s_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_d_scalar(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt d0, d1, d2\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD indexed dot accumulate ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_sudot_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sudot_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfdot_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.2s, v1.4h, v2.2h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfdot_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.4s, v1.8h, v2.2h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sdot_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sdot_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_usdot_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_usdot_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_udot_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_udot_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP widening/complex/indexed ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fmlal_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.2s, v1.2h, v2.2h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.4s, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.2s, v1.2h, v2.2h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.4s, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.2s, v1.2h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.4s, v1.4h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.2s, v1.2h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.4s, v1.4h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal2_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.2s, v1.2h, v2.2h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal2_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.4s, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl2_2s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.2s, v1.2h, v2.2h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl2_4s_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.4s, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal2_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.2s, v1.2h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlal2_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.4s, v1.4h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl2_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.2s, v1.2h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmlsl2_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.4s, v1.4h, v2.h[0]\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_rotation_0_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_rotation_90_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_rotation_180_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_rotation_270_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2s_rotation_0_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2s_rotation_90_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2s_rotation_180_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2s_rotation_270_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_rotation_0_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_rotation_90_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_rotation_180_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_rotation_270_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_rotation_0_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_rotation_90_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_rotation_180_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_rotation_270_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2d_rotation_0_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2d_rotation_90_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2d_rotation_180_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2d_rotation_270_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #270\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_indexed_rotation_0_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_indexed_rotation_90_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_indexed_rotation_180_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_2h_indexed_rotation_270_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_indexed_rotation_0_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_indexed_rotation_90_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_indexed_rotation_180_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4h_indexed_rotation_270_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_indexed_rotation_0_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_indexed_rotation_90_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_indexed_rotation_180_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #180\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmla_4s_indexed_rotation_270_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #270\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2h_rotation_90(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4h, v1.4h, v2.4h, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2h_rotation_270(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4h, v1.4h, v2.4h, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2s_rotation_90(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2s, v1.2s, v2.2s, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2s_rotation_270(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2s, v1.2s, v2.2s, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_4h_rotation_90(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.8h, v1.8h, v2.8h, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_4h_rotation_270(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.8h, v1.8h, v2.8h, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_4s_rotation_90(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4s, v1.4s, v2.4s, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_4s_rotation_270(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4s, v1.4s, v2.4s, #270\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2d_rotation_90(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2d, v1.2d, v2.2d, #90\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcadd_2d_rotation_270(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2d, v1.2d, v2.2d, #270\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmla_2h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_4h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_2d_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_2h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_2s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_4h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_4s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_2d_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla h0, h1, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla s0, s1, v2.s[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmla_d_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla d0, d1, v2.d[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_h_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls h0, h1, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_s_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls s0, s1, v2.s[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmls_d_indexed_accumulate(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls d0, d1, v2.d[0]\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmul_2h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_2s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_4h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_4s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_2d_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_2h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_2s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_4h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_4s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_2d_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul h0, h1, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul s0, s1, v2.s[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_d_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul d0, d1, v2.d[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_h_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx h0, h1, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_s_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx s0, s1, v2.s[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_d_indexed(void *dst, const void *left, const void *element)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx d0, d1, v2.d[0]\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD logical immediate ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_orr_8b_immediate_accumulate(void *dst, const void *immediate)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\norr v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_orr_16b_immediate_accumulate(void *dst, const void *immediate)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\norr v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bic_8b_immediate_accumulate(void *dst, const void *immediate)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bic_16b_immediate_accumulate(void *dst, const void *immediate)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD BFMLAL accumulate ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_bfmlalb_indexed_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalb v0.4s, v1.8h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfmlalb_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalb v0.4s, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfmlalt_indexed_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalt v0.4s, v1.8h, v2.h[0]\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bfmlalt_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalt v0.4s, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD reverse ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_rev64_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.8b, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev64_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.4h, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev64_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev64_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.16b, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev64_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.8h, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev64_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev64 v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev16_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev16 v0.8b, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev16_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev16 v0.16b, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev32_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev32 v0.8b, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev32_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev32 v0.4h, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev32_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev32 v0.16b, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_rev32_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nrev32 v0.8h, v1.8h\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD integer reduce ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_saddlv_h_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsaddlv h0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saddlv_s_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsaddlv s0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saddlv_h_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsaddlv h0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saddlv_s_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsaddlv s0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_saddlv_d_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsaddlv d0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_smaxv_b_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsmaxv b0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsmaxv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxv_b_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsmaxv b0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsmaxv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_smaxv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsmaxv s0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sminv_b_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsminv b0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsminv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminv_b_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsminv b0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsminv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sminv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsminv s0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_addv_b_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\naddv b0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\naddv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addv_b_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\naddv b0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\naddv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_addv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\naddv s0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uaddlv_h_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuaddlv h0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaddlv_s_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuaddlv s0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaddlv_h_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuaddlv h0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaddlv_s_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuaddlv s0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uaddlv_d_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuaddlv d0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_umaxv_b_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numaxv b0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numaxv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxv_b_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numaxv b0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numaxv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_umaxv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numaxv s0, v1.4s\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_uminv_b_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numinv b0, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numinv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminv_b_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numinv b0, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numinv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uminv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\numinv s0, v1.4s\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD narrow ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_xtn_8b_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nxtn v0.8b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_xtn_4h_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nxtn v0.4h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_xtn_2s_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nxtn v0.2s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_8b_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.8b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_4h_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.4h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_2s_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.2s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_8b_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.8b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_4h_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.4h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_2s_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.2s, v1.2d\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_8b_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.8b, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_4h_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.4h, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_2s_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.2s, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_b_h_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn b0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_h_s_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn h0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtn_s_d_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtn s0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_b_h_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun b0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_h_s_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun h0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_sqxtun_s_d_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nsqxtun s0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_b_h_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn b0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_h_s_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn h0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_uqxtn_s_d_scalar(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nuqxtn s0, d1\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP special vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_famax_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x0EC21C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famax_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x0EA2DC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famax_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EC21C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famax_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EA2DC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famax_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EE2DC20\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_famin_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EC21C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famin_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EA2DC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famin_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EC21C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famin_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EA2DC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_famin_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EE2DC20\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fscale_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EC23C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fscale_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EA2FC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fscale_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EC23C20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fscale_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EA2FC20\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fscale_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EE2FC20\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP unary vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fabs_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfabs v0.4h, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabs_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfabs v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabs_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfabs v0.8h, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabs_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfabs v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabs_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfabs v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fneg_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfneg v0.4h, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fneg_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfneg v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fneg_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfneg v0.8h, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fneg_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfneg v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fneg_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfneg v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fsqrt_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.4h, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsqrt_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.2s, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsqrt_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.8h, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsqrt_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.4s, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsqrt_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.2d, v1.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP binary vector ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fmaxnm_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnm_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnm_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnm_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnm_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fadd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fadd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fadd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fadd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fadd_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmulx_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmulx_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmax_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmax_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmax_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmax_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmax_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_frecps_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frecps_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fminnm_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnm_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnm_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnm_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnm_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fsub_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsub_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsub_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsub_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fsub_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmin_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmin_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmin_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmin_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmin_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_frsqrts_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_faddp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmul_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmul_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmge_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_facge_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facge_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fmaxp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fdiv_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fdiv_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fdiv_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fdiv_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fdiv_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fminnmp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fabd_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fabd_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_facgt_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_facgt_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fminp_4h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.4h, v1.4h, v2.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminp_2s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.2s, v1.2s, v2.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminp_8h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.8h, v1.8h, v2.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminp_4s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.4s, v1.4s, v2.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminp_2d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.2d, v1.2d, v2.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP compare zero ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_h_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt h0, h1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_s_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt s0, s1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_d_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt d0, d1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_4h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.4h, v1.4h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_2s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.2s, v1.2s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_8h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.8h, v1.8h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_4s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.4s, v1.4s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmgt_2d_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.2d, v1.2d, #0.0\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_h_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq h0, h1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_s_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq s0, s1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_d_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq d0, d1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_4h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.4h, v1.4h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_2s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.2s, v1.2s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_8h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.8h, v1.8h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_4s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.4s, v1.4s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmeq_2d_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.2d, v1.2d, #0.0\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_h_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt h0, h1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_s_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt s0, s1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_d_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt d0, d1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_4h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.4h, v1.4h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_2s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.2s, v1.2s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_8h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.8h, v1.8h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_4s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.4s, v1.4s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmlt_2d_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.2d, v1.2d, #0.0\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmge_h_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge h0, h1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_s_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge s0, s1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_d_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge d0, d1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_4h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge v0.4h, v1.4h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_2s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge v0.2s, v1.2s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_8h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge v0.8h, v1.8h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_4s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge v0.4s, v1.4s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmge_2d_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmge v0.2d, v1.2d, #0.0\nstr q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void emu_template_simd_fcmle_h_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle h0, h1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_s_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle s0, s1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_d_scalar_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle d0, d1, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_4h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle v0.4h, v1.4h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_2s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle v0.2s, v1.2s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_8h_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle v0.8h, v1.8h, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_4s_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle v0.4s, v1.4s, #0.0\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fcmle_2d_zero(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfcmle v0.2d, v1.2d, #0.0\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD FP reduce ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxnmv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxnmv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxnmv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxnmv s0, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fmaxv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmaxv s0, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminnmv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminnmv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminnmv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminnmv s0, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminv_h_4h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminv h0, v1.4h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminv_h_8h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminv h0, v1.8h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_fminv_s_4s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfminv s0, v1.4s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_h_2h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfaddp h0, v1.2h\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_s_2s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfaddp s0, v1.2s\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_faddp_d_2d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfaddp d0, v1.2d\nstr q0, [x0]\nret\n"); }

/* ---------- SIMD logical ---------- */

ARM64_HW_TEMPLATE void emu_template_simd_mvn_8b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nmvn v0.8b, v1.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_mvn_16b(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nmvn v0.16b, v1.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_and_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nand v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_and_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nand v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bic_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bic_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_orr_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norr v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_orr_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norr v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_orn_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norn v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_orn_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norn v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_eor_8b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\neor v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_eor_16b(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\neor v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bsl_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbsl v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bsl_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbsl v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bit_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbit v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bit_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbit v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bif_8b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbif v0.8b, v1.8b, v2.8b\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_simd_bif_16b_accumulate(void *dst, const void *left, const void *right)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbif v0.16b, v1.16b, v2.16b\nstr q0, [x0]\nret\n"); }

/* ---------- Scalar FP binary ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_fmul_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmul_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmul_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fdiv_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fdiv_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fdiv_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fadd_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fadd_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fadd_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsub_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsub_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsub_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmax_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmax_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmax_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmin_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmin_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmin_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmaxnm_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmaxnm_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmaxnm_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fminnm_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fminnm_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fminnm_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm d0, d1, d2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmul_h(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul h0, h1, h2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmul_s(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul s0, s1, s2\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmul_d(void *dst, const void *left, const void *right)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul d0, d1, d2\nstr q0, [x0]\nret\n"); }

/* ---------- Scalar FP unary ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_fmov_h(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmov h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmov_s(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmov s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmov_d(void *dst, const void *source)
{ asm volatile("ldr q1, [x1]\nfmov d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fabs_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fabs_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fabs_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fneg_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fneg_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fneg_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsqrt_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsqrt_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fsqrt_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintn_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintn_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintn_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintp_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintp_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintp_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintm_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintm_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintm_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintz_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintz_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintz_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinta_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinta_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinta_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintx_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintx_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frintx_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx d0, d1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinti_h_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti h0, h1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinti_s_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti s0, s1\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_frinti_d_merge(void *dst, const void *source)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti d0, d1\nstr q0, [x0]\nret\n"); }

/* ---------- Scalar FP ternary ---------- */

ARM64_HW_TEMPLATE void emu_template_fp_fmadd_h(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd h0, h1, h2, h3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmadd_s(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd s0, s1, s2, s3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmadd_d(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd d0, d1, d2, d3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmsub_h(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub h0, h1, h2, h3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmsub_s(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub s0, s1, s2, s3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fmsub_d(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub d0, d1, d2, d3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmadd_h(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd h0, h1, h2, h3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmadd_s(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd s0, s1, s2, s3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmadd_d(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd d0, d1, d2, d3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmsub_h(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub h0, h1, h2, h3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmsub_s(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub s0, s1, s2, s3\nstr q0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void emu_template_fp_fnmsub_d(void *dst, const void *left, const void *right, const void *addend)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub d0, d1, d2, d3\nstr q0, [x0]\nret\n"); }

/* ---------- Scalar FP compare ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_h(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp h1, h2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_s(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp s1, s2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_d(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp d1, d2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_h(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe h1, h2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_s(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe s1, s2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_d(const void *left, const void *right)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe d1, d2\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_zero_h(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmp h1, #0.0\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_zero_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmp s1, #0.0\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmp_zero_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmp d1, #0.0\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_zero_h(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmpe h1, #0.0\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_zero_s(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmpe s1, #0.0\nmrs x0, nzcv\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_fp_fcmpe_zero_d(const void *source)
{ asm volatile("ldr q1, [x0]\nfcmpe d1, #0.0\nmrs x0, nzcv\nret\n"); }

/* ======================== 数据处理立即数指令模板 ======================== */

/* ---------- ADD/SUB：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_add_w32(uint64_t left, uint64_t right)
{ asm volatile("add w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_add_w64(uint64_t left, uint64_t right)
{ asm volatile("add x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_adds_w32(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("adds w0, w0, w1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_adds_w64(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("adds x0, x0, x1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sub_w32(uint64_t left, uint64_t right)
{ asm volatile("sub w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sub_w64(uint64_t left, uint64_t right)
{ asm volatile("sub x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_subs_w32(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("subs w0, w0, w1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_subs_w64(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("subs x0, x0, x1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }

/* ---------- 逻辑运算：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_and_w32(uint64_t left, uint64_t right)
{ asm volatile("and w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_and_w64(uint64_t left, uint64_t right)
{ asm volatile("and x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bic_w32(uint64_t left, uint64_t right)
{ asm volatile("bic w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bic_w64(uint64_t left, uint64_t right)
{ asm volatile("bic x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_orr_w32(uint64_t left, uint64_t right)
{ asm volatile("orr w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_orr_w64(uint64_t left, uint64_t right)
{ asm volatile("orr x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_orn_w32(uint64_t left, uint64_t right)
{ asm volatile("orn w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_orn_w64(uint64_t left, uint64_t right)
{ asm volatile("orn x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_eor_w32(uint64_t left, uint64_t right)
{ asm volatile("eor w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_eor_w64(uint64_t left, uint64_t right)
{ asm volatile("eor x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_eon_w32(uint64_t left, uint64_t right)
{ asm volatile("eon w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_eon_w64(uint64_t left, uint64_t right)
{ asm volatile("eon x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ands_w32(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("ands w0, w0, w1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ands_w64(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("ands x0, x0, x1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bics_w32(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("bics w0, w0, w1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bics_w64(uint64_t left, uint64_t right, uint64_t *nzcv)
{ asm volatile("bics x0, x0, x1\nmrs x3, nzcv\nstr x3, [x2]\nret\n"); }

/* ---------- MIN/MAX：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_smax_w32(uint64_t left, uint64_t right)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, gt\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smax_w64(uint64_t left, uint64_t right)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, gt\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umax_w32(uint64_t left, uint64_t right)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, hi\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umax_w64(uint64_t left, uint64_t right)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, hi\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smin_w32(uint64_t left, uint64_t right)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, lt\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smin_w64(uint64_t left, uint64_t right)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, lt\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umin_w32(uint64_t left, uint64_t right)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, lo\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umin_w64(uint64_t left, uint64_t right)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, lo\nret\n"); }

/* ---------- EXTR 与位域 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_extract_w32(uint64_t high, uint64_t low, uint32_t shift)
{ asm volatile("neg w3, w2\nlslv w3, w0, w3\nlsrv w0, w1, w2\ncmp w2, #0\ncsel w3, wzr, w3, eq\norr w0, w0, w3\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_extract_w64(uint64_t high, uint64_t low, uint32_t shift)
{ asm volatile("neg x3, x2\nlslv x3, x0, x3\nlsrv x0, x1, x2\ncmp x2, #0\ncsel x3, xzr, x3, eq\norr x0, x0, x3\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbfm_w32(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv w0, w0, w2\nand w0, w0, w3\nadd w5, w4, #1\nlsr w5, w5, #1\ncmp w5, #0\nmov w6, #0x80000000\ncsel w5, w6, w5, eq\nand w6, w0, w4\nmvn w7, w4\norr w7, w6, w7\ntst w0, w5\ncsel w0, w7, w6, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbfm_w64(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv x0, x0, x2\nand x0, x0, x3\nadd x5, x4, #1\nlsr x5, x5, #1\ncmp x5, #0\nmov x6, #0x8000000000000000\ncsel x5, x6, x5, eq\nand x6, x0, x4\nmvn x7, x4\norr x7, x6, x7\ntst x0, x5\ncsel x0, x7, x6, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bfm_w32(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv w0, w0, w2\nand w0, w0, w3\nand w5, w3, w4\nbic w1, w1, w5\nand w0, w0, w5\norr w0, w1, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_bfm_w64(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv x0, x0, x2\nand x0, x0, x3\nand x5, x3, x4\nbic x1, x1, x5\nand x0, x0, x5\norr x0, x1, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ubfm_w32(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv w0, w0, w2\nand w0, w0, w3\nand w0, w0, w4\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_ubfm_w64(uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask)
{ asm volatile("rorv x0, x0, x2\nand x0, x0, x3\nand x0, x0, x4\nret\n"); }

/* ---------- Move wide ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_movn_w32(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv w0, w1, w2\nmvn w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_movn_w64(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv x0, x1, x2\nmvn x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_movz_w32(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv w0, w1, w2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_movz_w64(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv x0, x1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_movk_w32(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv w1, w1, w2\nmov w3, #0xffff\nlslv w3, w3, w2\nbic w0, w0, w3\norr w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_movk_w64(uint64_t dst, uint64_t immediate, uint32_t shift)
{ asm volatile("lslv x1, x1, x2\nmov x3, #0xffff\nlslv x3, x3, x2\nbic x0, x0, x3\norr x0, x0, x1\nret\n"); }

/* ======================== 数据处理寄存器指令模板 ======================== */

/* ---------- 条件选择 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_csel_w32(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsel w0, w0, w1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csel_w64(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsel x0, x0, x1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csinc_w32(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsinc w0, w0, w1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csinc_w64(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsinc x0, x0, x1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csinv_w32(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsinv w0, w0, w1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csinv_w64(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsinv x0, x0, x1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csneg_w32(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsneg w0, w0, w1, ne\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_csneg_w64(uint64_t left, uint64_t right, uint32_t take)
{ asm volatile("cmp w2, #0\ncsneg x0, x0, x1, ne\nret\n"); }

/* ---------- 扩展寄存器：立即数 MIN/MAX 复用 SXT byte 模板 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_uxtb_shift(uint64_t value, uint32_t shift)
{ asm volatile("uxtb w0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_uxth_shift(uint64_t value, uint32_t shift)
{ asm volatile("uxth w0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_uxtw_shift(uint64_t value, uint32_t shift)
{ asm volatile("mov w0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_uxtx_shift(uint64_t value, uint32_t shift)
{ asm volatile("lslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sxtb_shift(uint64_t value, uint32_t shift)
{ asm volatile("sxtb x0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sxth_shift(uint64_t value, uint32_t shift)
{ asm volatile("sxth x0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sxtw_shift(uint64_t value, uint32_t shift)
{ asm volatile("sxtw x0, w0\nlslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sxtx_shift(uint64_t value, uint32_t shift)
{ asm volatile("lslv x0, x0, x1\nret\n"); }

/* ---------- 带进位 ADD/SUB ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_adc_w32(uint64_t left, uint64_t right, uint64_t input_nzcv)
{ asm volatile("msr nzcv, x2\nadc w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_adc_w64(uint64_t left, uint64_t right, uint64_t input_nzcv)
{ asm volatile("msr nzcv, x2\nadc x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_adcs_w32(uint64_t left, uint64_t right, uint64_t input_nzcv, uint64_t *nzcv)
{ asm volatile("msr nzcv, x2\nadcs w0, w0, w1\nmrs x4, nzcv\nstr x4, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_adcs_w64(uint64_t left, uint64_t right, uint64_t input_nzcv, uint64_t *nzcv)
{ asm volatile("msr nzcv, x2\nadcs x0, x0, x1\nmrs x4, nzcv\nstr x4, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbc_w32(uint64_t left, uint64_t right, uint64_t input_nzcv)
{ asm volatile("msr nzcv, x2\nsbc w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbc_w64(uint64_t left, uint64_t right, uint64_t input_nzcv)
{ asm volatile("msr nzcv, x2\nsbc x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbcs_w32(uint64_t left, uint64_t right, uint64_t input_nzcv, uint64_t *nzcv)
{ asm volatile("msr nzcv, x2\nsbcs w0, w0, w1\nmrs x4, nzcv\nstr x4, [x3]\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sbcs_w64(uint64_t left, uint64_t right, uint64_t input_nzcv, uint64_t *nzcv)
{ asm volatile("msr nzcv, x2\nsbcs x0, x0, x1\nmrs x4, nzcv\nstr x4, [x3]\nret\n"); }

/* ---------- 移位与位操作 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_lslv_w32(uint64_t value, uint64_t amount)
{ asm volatile("lslv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lslv_w64(uint64_t value, uint64_t amount)
{ asm volatile("lslv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lsrv_w32(uint64_t value, uint64_t amount)
{ asm volatile("lsrv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_lsrv_w64(uint64_t value, uint64_t amount)
{ asm volatile("lsrv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_asrv_w32(uint64_t value, uint64_t amount)
{ asm volatile("asrv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_asrv_w64(uint64_t value, uint64_t amount)
{ asm volatile("asrv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rorv_w32(uint64_t value, uint64_t amount)
{ asm volatile("rorv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rorv_w64(uint64_t value, uint64_t amount)
{ asm volatile("rorv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rbit_w32(uint64_t value)
{ asm volatile("rbit w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rbit_w64(uint64_t value)
{ asm volatile("rbit x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rev16_w32(uint64_t value)
{ asm volatile("rev16 w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rev16_w64(uint64_t value)
{ asm volatile("rev16 x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rev32_w32(uint64_t value)
{ asm volatile("rev w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rev32_w64(uint64_t value)
{ asm volatile("rev32 x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_rev64_w64(uint64_t value)
{ asm volatile("rev x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_clz_w32(uint64_t value)
{ asm volatile("clz w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_clz_w64(uint64_t value)
{ asm volatile("clz x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_cls_w32(uint64_t value)
{ asm volatile("cls w0, w0\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_cls_w64(uint64_t value)
{ asm volatile("cls x0, x0\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_cnt_w32(uint64_t value)
{ asm volatile("movi v0.2d, #0\nfmov s0, w0\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov w0, v0.b[0]\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_cnt_w64(uint64_t value)
{ asm volatile("movi v0.2d, #0\nfmov d0, x0\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov w0, v0.b[0]\nret\n"); }

/* ---------- CRC ---------- */

ARM64_HW_TEMPLATE uint32_t emu_template_crc32b(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32b w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32h(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32h w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32w(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32w w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32x(uint32_t accumulator, uint64_t value)
{ asm volatile("crc32x w0, w0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32cb(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32cb w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32ch(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32ch w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32cw(uint32_t accumulator, uint32_t value)
{ asm volatile("crc32cw w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint32_t emu_template_crc32cx(uint32_t accumulator, uint64_t value)
{ asm volatile("crc32cx w0, w0, x1\nret\n"); }

/* ---------- 除法 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_udiv_w32(uint64_t left, uint64_t right)
{ asm volatile("udiv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_udiv_w64(uint64_t left, uint64_t right)
{ asm volatile("udiv x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sdiv_w32(uint64_t left, uint64_t right)
{ asm volatile("sdiv w0, w0, w1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_sdiv_w64(uint64_t left, uint64_t right)
{ asm volatile("sdiv x0, x0, x1\nret\n"); }

/* ---------- 乘法 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_madd_w32(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("madd w0, w0, w1, w2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_madd_w64(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("madd x0, x0, x1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_msub_w32(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("msub w0, w0, w1, w2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_msub_w64(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("msub x0, x0, x1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smaddl(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("smaddl x0, w0, w1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smsubl(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("smsubl x0, w0, w1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_smulh(uint64_t left, uint64_t right)
{ asm volatile("smulh x0, x0, x1\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umaddl(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("umaddl x0, w0, w1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umsubl(uint64_t left, uint64_t right, uint64_t addend)
{ asm volatile("umsubl x0, w0, w1, x2\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_umulh(uint64_t left, uint64_t right)
{ asm volatile("umulh x0, x0, x1\nret\n"); }

/* ---------- 一元整数 ---------- */

ARM64_HW_TEMPLATE uint64_t emu_template_abs_w32(uint64_t value)
{ asm volatile("cmp w0, #0\ncneg w0, w0, mi\nret\n"); }
ARM64_HW_TEMPLATE uint64_t emu_template_abs_w64(uint64_t value)
{ asm volatile("cmp x0, #0\ncneg x0, x0, mi\nret\n"); }

// clang-format on

#undef ARM64_HW_TEMPLATE

#endif