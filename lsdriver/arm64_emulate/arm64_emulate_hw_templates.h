#ifndef ARM64_EMULATE_HW_TEMPLATES_H
#define ARM64_EMULATE_HW_TEMPLATES_H

#include <linux/types.h>

#define ARM64_HW_TEMPLATE static __attribute__((__naked__, __noinline__, __unused__, __section__(".text.arm64_hw_templates")))

/*
快速解释硬件汇编模板执行模型：

1. executor leaf 根据原机器码中的 Rd/Rn/Rm/Ra，从 pt_regs 或 fp_regs 读取
	对应的软件寄存器现场。原指令的寄存器编号只用于选择软件现场槽位，不直接
	决定模板使用的硬件寄存器编号。

2. 所有模板统一采用六个参数的 void ABI。前五个参数是固定寄存器值输入，
	放入 x0-x4；最后一个参数是固定输出指针，放入 x5。无输入或无输出的
	模板仍保留全部参数，未使用的参数由调用方传 0。输出指针只指向
	executor 的临时输出区，不指向 pt_regs 或 fp_regs。需要多个结果时，
	模板按该指令族约定写入输出区的固定槽位。
	通用运算固定借用 x0-x5；FP/Advanced SIMD 运算固定借用 v0-v3 完成
	装载、运算和保存。因此不需要为原指令所有 Rd/Rn/Rm/Ra 组合生成模板，
	只保留操作、数据形态和编码立即数等真正影响机器码的变体。

3. 模板不会自动把被修改的参数寄存器同步到软件现场。所有架构结果均通过
	固定输出指针写入临时结果槽，再由 leaf 使用 write_gpr_or_zr()、write_gpr_or_sp()、
	write_nzcv() 等显式提交到 pt_regs。FP/Advanced SIMD 模板同样先写入
	临时结果槽，不直接访问 fp_regs。

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

ARM64_HW_TEMPLATE void clrex_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #0\nret\n"); }
ARM64_HW_TEMPLATE void clrex_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #1\nret\n"); }
ARM64_HW_TEMPLATE void clrex_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #2\nret\n"); }
ARM64_HW_TEMPLATE void clrex_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #3\nret\n"); }
ARM64_HW_TEMPLATE void clrex_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #4\nret\n"); }
ARM64_HW_TEMPLATE void clrex_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #5\nret\n"); }
ARM64_HW_TEMPLATE void clrex_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #6\nret\n"); }
ARM64_HW_TEMPLATE void clrex_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #7\nret\n"); }
ARM64_HW_TEMPLATE void clrex_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #8\nret\n"); }
ARM64_HW_TEMPLATE void clrex_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #9\nret\n"); }
ARM64_HW_TEMPLATE void clrex_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #10\nret\n"); }
ARM64_HW_TEMPLATE void clrex_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #11\nret\n"); }
ARM64_HW_TEMPLATE void clrex_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #12\nret\n"); }
ARM64_HW_TEMPLATE void clrex_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #13\nret\n"); }
ARM64_HW_TEMPLATE void clrex_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #14\nret\n"); }
ARM64_HW_TEMPLATE void clrex_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clrex #15\nret\n"); }

/* ---------- DSB ---------- */

ARM64_HW_TEMPLATE void dsb_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #0\nret\n"); }
ARM64_HW_TEMPLATE void dsb_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #1\nret\n"); }
ARM64_HW_TEMPLATE void dsb_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #2\nret\n"); }
ARM64_HW_TEMPLATE void dsb_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #3\nret\n"); }
ARM64_HW_TEMPLATE void dsb_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #4\nret\n"); }
ARM64_HW_TEMPLATE void dsb_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #5\nret\n"); }
ARM64_HW_TEMPLATE void dsb_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #6\nret\n"); }
ARM64_HW_TEMPLATE void dsb_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #7\nret\n"); }
ARM64_HW_TEMPLATE void dsb_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #8\nret\n"); }
ARM64_HW_TEMPLATE void dsb_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #9\nret\n"); }
ARM64_HW_TEMPLATE void dsb_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #10\nret\n"); }
ARM64_HW_TEMPLATE void dsb_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #11\nret\n"); }
ARM64_HW_TEMPLATE void dsb_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #12\nret\n"); }
ARM64_HW_TEMPLATE void dsb_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #13\nret\n"); }
ARM64_HW_TEMPLATE void dsb_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #14\nret\n"); }
ARM64_HW_TEMPLATE void dsb_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dsb #15\nret\n"); }

/* ---------- DMB ---------- */

ARM64_HW_TEMPLATE void dmb_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #0\nret\n"); }
ARM64_HW_TEMPLATE void dmb_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #1\nret\n"); }
ARM64_HW_TEMPLATE void dmb_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #2\nret\n"); }
ARM64_HW_TEMPLATE void dmb_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #3\nret\n"); }
ARM64_HW_TEMPLATE void dmb_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #4\nret\n"); }
ARM64_HW_TEMPLATE void dmb_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #5\nret\n"); }
ARM64_HW_TEMPLATE void dmb_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #6\nret\n"); }
ARM64_HW_TEMPLATE void dmb_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #7\nret\n"); }
ARM64_HW_TEMPLATE void dmb_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #8\nret\n"); }
ARM64_HW_TEMPLATE void dmb_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #9\nret\n"); }
ARM64_HW_TEMPLATE void dmb_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #10\nret\n"); }
ARM64_HW_TEMPLATE void dmb_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #11\nret\n"); }
ARM64_HW_TEMPLATE void dmb_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #12\nret\n"); }
ARM64_HW_TEMPLATE void dmb_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #13\nret\n"); }
ARM64_HW_TEMPLATE void dmb_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #14\nret\n"); }
ARM64_HW_TEMPLATE void dmb_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dmb #15\nret\n"); }

/* ---------- ISB ---------- */

ARM64_HW_TEMPLATE void isb_hw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("isb\nret\n"); }

/* ---------- YIELD ---------- */

ARM64_HW_TEMPLATE void yield_hw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("yield\nret\n"); }

/* ======================== 访存指令模板 ======================== */

/* ---------- GPR load ---------- */

ARM64_HW_TEMPLATE void ldrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsw x0, [x0]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_literal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_literal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_literal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsw x0, [x0]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldurb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldurh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldursb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldursb x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldursh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldursh x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldursw x0, [x0]\nstr x0, [x5]\nret\n"); }

/* ---------- unprivileged GPR load ---------- */

ARM64_HW_TEMPLATE void ldtrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtr w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtr x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrsb w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrsb x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrsh w0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrsh x0, [x0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldtrsw x0, [x0]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void sturb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sturb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sturh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sturh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stur w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stur x1, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void sttrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sttrb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sttrh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sttr w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sttr x1, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void strb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("strb w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void strh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("strh w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("str w1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("str x1, [x0]\nret\n"); }

/* ---------- GPR hardware address generation ---------- */

ARM64_HW_TEMPLATE void ldr_addr_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_addr_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrb w2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrh w2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr w2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr x2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_addr_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrb w2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldur_addr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldur b0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldur h0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldur s0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldur d0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldur q0, [x3]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ld1_addr_simd_element_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ld1_addr_simd_element_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ld1_addr_simd_element_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ld1_addr_simd_element_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void stur_addr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstur b0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstur h0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstur s0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstur d0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstur q0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrh w2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr w2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr x2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldrsb_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldrsw x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldrsb_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsb w2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsb x2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsh w2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsh x2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldrsw x2, [x0]\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldrsb_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb w2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb x2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh w2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh x2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsw x2, [x0]\nadd x0, x0, x1\nstr x2, [x5]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void str_addr_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstrb w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstrh w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr x2, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void str_addr_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstrb w2, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstrh w2, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr w2, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr x2, [x0]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void str_addr_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("strb w2, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("strh w2, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("str w2, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("str x2, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldurb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldurh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldur w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldur x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursb_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldursb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursb_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldursb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursh_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldursh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursh_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldursh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldursw_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldursw x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void stur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsturb w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsturh w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstur w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstur x2, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void ldtr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsb_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsb_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsh_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsh_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldtrsw_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldtrsw x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void sttr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsttrb w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsttrh w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsttr w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void sttr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nsttr x2, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_reg_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldr x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldrsb_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsb_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsh_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsw x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsw x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x1, x0, x1\nldrsw x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldrsw_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x1, x0, x1\nldrsw x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void str_reg_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstrb w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstrh w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstr w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstr x2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstrb w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstrh w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstr w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstr x2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstrb w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstrh w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstr w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x3\nadd x1, x0, x1\nstr x2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstrb w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstrh w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstr w2, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x3\nadd x1, x0, x1\nstr x2, [x1]\nret\n"); }

ARM64_HW_TEMPLATE void add_addr_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void add_addr_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void add_addr_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void add_addr_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void add_addr_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x0, x0, x1\nstr x0, [x5]\nret\n"); }

/* ---------- conditional select ---------- */

ARM64_HW_TEMPLATE void csel_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsel w0, w0, w1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csel_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsel x0, x0, x1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csinc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsinc w0, w0, w1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csinc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsinc x0, x0, x1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csinv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsinv w0, w0, w1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csinv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsinv x0, x0, x1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csneg_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsneg w0, w0, w1, ne\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void csneg_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w2, #0\ncsneg x0, x0, x1, ne\nstr x0, [x5]\nret\n"); }

/* ---------- register extend and shift ---------- */

ARM64_HW_TEMPLATE void uxtb_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtb w0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void uxth_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxth w0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void uxtw_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov w0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void uxtx_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sxtb_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtb x0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sxth_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxth x0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sxtw_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x0, w0\nlslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sxtx_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x0, x0, x1\nstr x0, [x5]\nret\n"); }

/* ---------- PRFM ---------- */

ARM64_HW_TEMPLATE void prfm_addr_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nmov x6, x30\nblr x4\nmov x30, x6\nret\n"); }
ARM64_HW_TEMPLATE void prfm_addr_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x0, x0, x1\nmov x6, x30\nblr x4\nmov x30, x6\nret\n"); }
ARM64_HW_TEMPLATE void prfm_addr_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x0, x0, x1\nmov x6, x30\nblr x4\nmov x30, x6\nret\n"); }
ARM64_HW_TEMPLATE void prfm_addr_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x0, x0, x1\nmov x6, x30\nblr x4\nmov x30, x6\nret\n"); }
ARM64_HW_TEMPLATE void prfm_addr_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x0, x0, x1\nmov x6, x30\nblr x4\nmov x30, x6\nret\n"); }

ARM64_HW_TEMPLATE void prfm_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #3, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #4, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #5, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #6, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #7, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #8, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #9, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #10, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #11, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #12, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #13, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #14, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #15, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_16(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #16, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_17(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #17, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_18(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #18, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_19(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #19, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_20(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #20, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_21(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #21, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_22(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #22, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_23(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #23, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_24(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #24, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_25(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #25, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_26(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #26, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_27(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #27, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_28(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #28, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_29(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #29, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_30(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #30, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void prfm_31(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("prfm #31, [x0]\nret\n"); }

/* ---------- RCpc ---------- */

ARM64_HW_TEMPLATE void ldapur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapurb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapurh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapur w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapur x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_sb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapursb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_sb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapursb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_sh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapursh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_sh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapursh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_addr_sw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapursw x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlurb w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlurh w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlur w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlur x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldaprb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldaprh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldapr x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldapurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapurb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapurh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapur w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapur x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapursb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapursb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapursb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapursb x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapursh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapursh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapursh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapursh x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapursw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapursw x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void stlurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlurb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlurh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlur w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlur x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldaprb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaprb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaprh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaprh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldapr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldapr x0, [x1]\nstr x0, [x5]\nret\n"); }

/* ---------- ordered ---------- */

ARM64_HW_TEMPLATE void ldlar_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldlarb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlar_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldlarh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlar_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldlar w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlar_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldlar x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldarb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldarh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldar w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x1, x0, x1\nldar x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstllrb w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstllrh w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstllr w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstllr x2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlrb w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlrh w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlr w2, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nstlr x2, [x3]\nret\n"); }

ARM64_HW_TEMPLATE void ldlarb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldlarb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlarh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldlarh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlar_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldlar w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldlar_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldlar x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldarb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldarb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldarh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldarh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldar w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldar_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldar x0, [x1]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void stllrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stllrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stllr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstllr x0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stlrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlrb w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stlrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlrh w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlr w0, [x1]\nret\n"); }
ARM64_HW_TEMPLATE void stlr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nmov x0, x1\nmov x1, x2\nstlr x0, [x1]\nret\n"); }

/* ---------- exclusive ---------- */

ARM64_HW_TEMPLATE void ldxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldxrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaxrb w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldxrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaxrh w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldxr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaxr w0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldxr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x1, x0\nldaxr x0, [x1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldxp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldaxp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldxp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldaxp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }

ARM64_HW_TEMPLATE void stxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstxrb w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstlxrb w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstxrh w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstlxrh w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstxr w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstlxr w0, w1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstxr w0, x1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nstlxr w0, x1, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nstxp w0, w1, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nstlxp w0, w1, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nstxp w0, x1, x2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nstlxp w0, x1, x2, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldxrb w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldxrh w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldxr w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldxr x0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldaxrb w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldaxrh w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldaxr w0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nldaxr x0, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldxp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldxp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldaxp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldaxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldaxp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxrb w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxrh w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxr w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxr w0, x2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxrb w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxrh w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxr w0, w2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxr w0, x2, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxp w0, w2, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstxp w0, x2, x3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxp w0, w2, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void stlxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nstlxp w0, x2, x3, [x4]\nstr x0, [x5]\nret\n"); }

/* ---------- LSE RMW ---------- */

ARM64_HW_TEMPLATE void ldadd_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldadd w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldadd x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddab w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddah w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldadda w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldadda x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddlb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddlh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddl w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddl x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddalb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddalh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddal w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldaddal x2, x0, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldclr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclr w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclr x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrab w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrah w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclra w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclra x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrlb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrlh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrl w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclrl x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclralb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclralh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclral w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldclral x2, x0, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldeor_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeor w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeor x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorab w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorah w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeora w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeora x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorlb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorlh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorl w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeorl x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeoralb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeoralh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeoral w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldeoral x2, x0, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldset_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldseth w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldset w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldset x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetab w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetah w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldseta w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldseta x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetlb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetlh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetl w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetl x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetalb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetalh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetal w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nldsetal x2, x0, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void swp_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswph w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswp w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswp x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpab w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpah w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpa w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpa x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswplb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswplh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpl w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpl x2, x0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpalb w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpalh w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpal w2, w0, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x3, x0, x1\nswpal x2, x0, [x3]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldadd_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldadd w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadd_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldadd x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldadda w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldadda_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldadda x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldaddal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldaddal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldclr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclr w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclr x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclra w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclra_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclra x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclrl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclrl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclralb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclralh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclral w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldclral_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldclral x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldeor_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeor w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeor_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeor x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeora w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeora_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeora x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeorl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeorl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeoralb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeoralh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeoral w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldeoral_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldeoral x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldset_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldseth w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldset w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldset_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldset x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldseta w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldseta_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldseta x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsetal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsetal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldsmax_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmax_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmax w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmax_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmax x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxa w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxa x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmaxal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmaxal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldsmin_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmin_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmin w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmin_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmin x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmina_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmina_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmina_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmina w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsmina_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsmina x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldsminal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldsminal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldumax_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumax_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumax w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumax_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumax x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxa w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxa x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumaxal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumaxal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldumin_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumin_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumin w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumin_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumin x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumina_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumina_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumina_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumina w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldumina_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nldumina x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminlb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminlh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lduminal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nlduminal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void swp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswph w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswp w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswp x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpab w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpah w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpa w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpa x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswplb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswplh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpl w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpl x1, x0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpalb w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpalh w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpal w1, w0, [x2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void swpal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x2, x0\nswpal x1, x0, [x2]\nstr x0, [x5]\nret\n"); }

/* ---------- CAS ---------- */

ARM64_HW_TEMPLATE void cas_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasb w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncash w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncas w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov x0, x1\ncas x0, x2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasab w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasah w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasa w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasa x0, x2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncaslb w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncaslh w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasl w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasl x0, x2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasalb w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasalh w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov w0, w1\ncasal w0, w2, [x3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x3, x0\nmov x0, x1\ncasal x0, x2, [x3]\nstr x0, [x5]\nret\n"); }

/* ---------- CASP ---------- */

ARM64_HW_TEMPLATE void casp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncasp w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncasp x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspa w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspa x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspl w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspl x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspal w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("mov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspal x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void cas_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasb w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncash w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncas w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cas_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov x0, x2\ncas x0, x3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasab w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasah w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasa w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov x0, x2\ncasa x0, x3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncaslb w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncaslh w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasl w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov x0, x2\ncasl x0, x3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasalb w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasalh w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov w0, w2\ncasal w0, w3, [x4]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x4, x0, x1\nmov x0, x2\ncasal x0, x3, [x4]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void casp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncasp w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void casp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncasp x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspa w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspa x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspl w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspl x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspal w0, w1, w2, w3, [x6]\nstp x0, x1, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void caspal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspal x0, x1, x2, x3, [x6]\nstp x0, x1, [x5]\nret\n"); }

/* ---------- FP/SIMD load ---------- */

ARM64_HW_TEMPLATE void ldur_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur b0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur h0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur s0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur d0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldur_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldur q0, [x0]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr b0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr h0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr s0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr d0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_literal_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr s0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_literal_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr d0, [x0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_literal_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldr q0, [x0]\nstr q0, [x5]\nret\n"); }

/* ---------- FP/SIMD store ---------- */

ARM64_HW_TEMPLATE void stur_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstur b0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstur h0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstur s0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstur d0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stur_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstur q0, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void str_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstr b0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstr h0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstr s0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstr d0, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void str_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nstr q0, [x0]\nret\n"); }

/* ---------- FP/SIMD address-aware load ---------- */

ARM64_HW_TEMPLATE void ldr_addr_fp_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr b0, [x2]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr h0, [x2]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr s0, [x2]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr d0, [x2]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr q0, [x2]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_addr_fp_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr b0, [x2]\nstr q0, [x5]\nstr x2, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr h0, [x2]\nstr q0, [x5]\nstr x2, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr s0, [x2]\nstr q0, [x5]\nstr x2, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr d0, [x2]\nstr q0, [x5]\nstr x2, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x2, x0, x1\nldr q0, [x2]\nstr q0, [x5]\nstr x2, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void ldr_addr_fp_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr b0, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr h0, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr s0, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr d0, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr x0, [x5, #32]\nret\n"); }

/* ---------- FP/SIMD address-aware store ---------- */

ARM64_HW_TEMPLATE void str_addr_fp_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr b0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr h0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr s0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr d0, [x3]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr q0, [x3]\nret\n"); }

ARM64_HW_TEMPLATE void str_addr_fp_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr b0, [x3]\nstr x3, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr h0, [x3]\nstr x3, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr s0, [x3]\nstr x3, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr d0, [x3]\nstr x3, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nadd x3, x0, x1\nstr q0, [x3]\nstr x3, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void str_addr_fp_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nstr b0, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nstr h0, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nstr s0, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nstr d0, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void str_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nstr q0, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }

/* ---------- FP/SIMD register-offset load ---------- */

ARM64_HW_TEMPLATE void ldr_reg_fp_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr b0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr h0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_s_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr s0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_d_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr d0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_q_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("uxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr q0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr b0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr h0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_s_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr s0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_d_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr d0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_q_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr q0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr b0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr h0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_s_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr s0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_d_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr d0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_q_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sxtw x1, w1\nlsl x1, x1, x2\nadd x3, x0, x1\nldr q0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr b0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr h0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_s_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr s0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_d_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr d0, [x3]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ldr_reg_fp_q_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsl x1, x1, x2\nadd x3, x0, x1\nldr q0, [x3]\nstr q0, [x5]\nret\n"); }

/* ---------- FP/SIMD register-offset store ---------- */

ARM64_HW_TEMPLATE void str_reg_fp_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nuxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr b0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nuxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr h0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_s_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nuxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr s0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_d_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nuxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr d0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_q_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nuxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr q0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr b0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr h0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_s_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr s0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_d_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr d0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_q_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr q0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nsxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr b0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nsxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr h0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_s_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nsxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr s0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_d_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nsxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr d0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_q_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nsxtw x1, w1\nlsl x1, x1, x2\nadd x4, x0, x1\nstr q0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr b0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr h0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_s_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr s0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_d_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr d0, [x4]\nret\n"); }
ARM64_HW_TEMPLATE void str_reg_fp_q_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x3]\nlsl x1, x1, x2\nadd x4, x0, x1\nstr q0, [x4]\nret\n"); }

/* ---------- GPR pair load/store ---------- */

ARM64_HW_TEMPLATE void ldnp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldnp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldnp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldnp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldpsw_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldpsw x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }

ARM64_HW_TEMPLATE void stnp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stnp w1, w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stnp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stnp x1, x2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stp w1, w2, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stp x1, x2, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void ldp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldnp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldnp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldnp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldnp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldpsw x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_gpr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp w3, w4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldpsw x3, x4, [x0]\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_gpr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp w3, w4, [x0]\nadd x0, x0, x1\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp x3, x4, [x0]\nadd x0, x0, x1\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldpsw x3, x4, [x0]\nadd x0, x0, x1\nstr x3, [x5]\nstr x4, [x5, #8]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void stp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstp w2, w3, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstp x2, x3, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stnp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstnp w2, w3, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stnp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstnp x2, x3, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_gpr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstp w2, w3, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstp x2, x3, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_gpr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stp w2, w3, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("stp x2, x3, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }

/* ---------- FP/SIMD pair load/store ---------- */

ARM64_HW_TEMPLATE void ldnp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldnp s0, s1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldnp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldnp d0, d1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldnp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldnp q0, q1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp s0, s1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp d0, d1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp q0, q1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }

ARM64_HW_TEMPLATE void stnp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp s0, s1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stnp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp d0, d1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stnp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstnp q0, q1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp s0, s1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp d0, d1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x1]\nldr q1, [x2]\nstp q0, q1, [x0]\nret\n"); }

ARM64_HW_TEMPLATE void ldp_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp s0, s1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp d0, d1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp q0, q1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp s0, s1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp d0, d1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nldp q0, q1, [x0]\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp s0, s1, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp d0, d1, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void ldp_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldp q0, q1, [x0]\nadd x0, x0, x1\nstr q0, [x5]\nstr q1, [x5, #16]\nstr x0, [x5, #32]\nret\n"); }

ARM64_HW_TEMPLATE void stp_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp s0, s1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp d0, d1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp q0, q1, [x0]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp s0, s1, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp d0, d1, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nadd x0, x0, x1\nstp q0, q1, [x0]\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nstp s0, s1, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nstp d0, d1, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }
ARM64_HW_TEMPLATE void stp_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x2]\nldr q1, [x3]\nstp q0, q1, [x0]\nadd x0, x0, x1\nstr x0, [x5, #32]\nret\n"); }

/* ======================== FP 与 Advanced SIMD 指令模板 ======================== */

/* ---------- FP/AdvSIMD merge conversion ---------- */

ARM64_HW_TEMPLATE void fp_fcvt_d_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvt d0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvt_s_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvt s0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_xtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.16b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_xtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.8h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_xtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nxtn2 v0.4s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.16b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.8h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn2 v0.4s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.16b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.8h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun2 v0.4s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.16b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.8h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn2 v0.4s, v1.2d\nstr q0, [x5]\nret\n"); }

/* ---------- GPR to FP ---------- */

ARM64_HW_TEMPLATE void fp_scvtf_s_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nscvtf s0, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_ucvtf_s_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nucvtf s0, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_scvtf_d_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nscvtf d0, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_ucvtf_d_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nucvtf d0, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_scvtf_s_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nscvtf s0, x1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_ucvtf_s_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nucvtf s0, x1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_scvtf_d_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nscvtf d0, x1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_ucvtf_d_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nucvtf d0, x1\nstr q0, [x5]\nret\n"); }

/* ---------- FP/AdvSIMD conversion ---------- */

ARM64_HW_TEMPLATE void simd_fcvtns_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtns s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtns_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtns d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtns_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtns_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtns_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtns v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtms_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtms s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtms_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtms d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtms_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtms_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtms_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtms v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtas_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtas s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtas_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtas d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtas_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtas_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtas_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtas v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_scvtf_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nscvtf s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_scvtf_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nscvtf d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_scvtf_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nscvtf v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_scvtf_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nscvtf v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_scvtf_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nscvtf v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtps_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtps s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtps_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtps d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtps_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtps_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtps_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtps v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtzs_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzs s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzs_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzs d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzs v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtnu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtnu s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtnu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtnu d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtnu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtnu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtnu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtnu v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtmu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtmu s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtmu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtmu d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtmu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtmu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtmu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtmu v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtau_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtau s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtau_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtau d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtau_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtau_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtau_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtau v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_ucvtf_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nucvtf s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ucvtf_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nucvtf d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ucvtf_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nucvtf v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ucvtf_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nucvtf v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ucvtf_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nucvtf v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtpu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtpu s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtpu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtpu d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtpu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtpu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtpu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtpu v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcvtzu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzu s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcvtzu d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcvtzu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcvtzu v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

/* ---------- FP to GPR ---------- */

ARM64_HW_TEMPLATE void fp_fcvtns_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtns w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtns_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtns w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtns_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtns x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtns_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtns x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtnu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtnu w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtnu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtnu w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtnu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtnu x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtnu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtnu x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtas_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtas w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtas_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtas w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtas_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtas x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtas_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtas x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtau_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtau w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtau_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtau w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtau_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtau x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtau_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtau x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtps_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtps w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtps_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtps w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtps_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtps x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtps_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtps x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtpu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtpu w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtpu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtpu w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtpu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtpu x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtpu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtpu x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtms_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtms w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtms_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtms w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtms_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtms x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtms_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtms x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtmu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtmu w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtmu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtmu w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtmu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtmu x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtmu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtmu x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzs_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzs w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzs_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzs w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzs_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzs x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzs_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzs x0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzu w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzu w0, d1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzu x0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcvtzu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcvtzu x0, d1\nstr x0, [x5]\nret\n"); }

/* ---------- FP select ---------- */

ARM64_HW_TEMPLATE void fp_fcsel_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel h0, h1, h2, ne\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcsel_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel s0, s1, s2, ne\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcsel_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmp w3, #0\nfcsel d0, d1, d2, ne\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD lane and scalar transfer ---------- */

ARM64_HW_TEMPLATE void simd_extract_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrb w0, [x0, w1, uxtw]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrh w0, [x0, w1, uxtw #1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr w0, [x0, w1, uxtw #2]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr x0, [x0, w1, uxtw #3]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_signed_b_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb w0, [x0, w1, uxtw]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_signed_h_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh w0, [x0, w1, uxtw #1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_signed_b_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsb x0, [x0, w1, uxtw]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_signed_h_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsh x0, [x0, w1, uxtw #1]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_extract_signed_s_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldrsw x0, [x0, w1, uxtw #2]\nstr x0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_insert_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nstr q0, [x5]\nstrb w1, [x5, w2, uxtw]\nret\n"); }
ARM64_HW_TEMPLATE void simd_insert_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nstr q0, [x5]\nstrh w1, [x5, w2, uxtw #1]\nret\n"); }
ARM64_HW_TEMPLATE void simd_insert_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nstr q0, [x5]\nstr w1, [x5, w2, uxtw #2]\nret\n"); }
ARM64_HW_TEMPLATE void simd_insert_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nstr q0, [x5]\nstr x1, [x5, w2, uxtw #3]\nret\n"); }

ARM64_HW_TEMPLATE void simd_write_scalar_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("fmov s0, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_write_scalar_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("fmov d0, x1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_read_scalar_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfmov w0, s1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_read_scalar_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfmov x0, d1\nstr x0, [x5]\nret\n"); }

/* ---------- SIMD duplicate general ---------- */

ARM64_HW_TEMPLATE void simd_dup_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.8b, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.4h, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.2s, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.16b, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.8h, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.4s, w1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_dup_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("dup v0.2d, x1\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD fixed immediate templates ---------- */

ARM64_HW_TEMPLATE void simd_movi_d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x2f00e400\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_movi_2d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile(".inst 0x6f00e400\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_movi_4s_imm03(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile(".inst 0x4f000460\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_2s_imm70(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x0f03f600\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_2s_immf0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x0f07f600\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_s_imm60(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e2c1000\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_s_imme0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e3c1000\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_s_imm70(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e2e1000\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_s_immf0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e3e1000\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_d_imm78(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e6f1000\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmov_d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x1e601000\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_orr_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x4f001640\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_orr_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x0f001640\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bic_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x6f001640\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bic_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x2f001640\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mvni_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile(".inst 0x6f000640\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mvni_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x2f000640\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD RDM accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sqrdmlah_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlah_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlah_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlah_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlah_h_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlah_s_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlah s0, s1, s2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqrdmlsh_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlsh_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlsh_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlsh_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlsh_h_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmlsh_s_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmlsh s0, s1, s2\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD permute vector ---------- */

ARM64_HW_TEMPLATE void simd_uzp1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp1 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_trn1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn1 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_zip1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip1 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uzp2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uzp2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuzp2 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_trn2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_trn2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ntrn2 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_zip2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_zip2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nzip2 v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD integer accumulate vector ---------- */

ARM64_HW_TEMPLATE void simd_saba_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saba_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saba_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saba_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saba_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saba_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsaba v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_mla_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mla_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mla_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mla_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mla_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmla v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uaba_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaba_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaba_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaba_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaba_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaba_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuaba v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_mls_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mls_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mls_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mls_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mls_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mls_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nmls v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD saturating add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_sqadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqadd v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqsub v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqadd v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqsub v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD integer compare vector ---------- */

ARM64_HW_TEMPLATE void simd_cmgt_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmgt v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_cmge_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmge v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_cmtst_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmtst v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_cmhi_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhi v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_cmhs_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmhs v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_cmeq_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\ncmeq v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD variable shift vector ---------- */

ARM64_HW_TEMPLATE void simd_shl_4s_imm2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\n.inst 0x4f225400\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD extract ---------- */

ARM64_HW_TEMPLATE void simd_ext_8b_offset_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #4\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #5\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #6\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_8b_offset_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.8b, v1.8b, v2.8b, #7\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_ext_16b_offset_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #4\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #5\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #6\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #7\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #8\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #9\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #10\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #11\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #12\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #13\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #14\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ext_16b_offset_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\next v0.16b, v1.16b, v2.16b, #15\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_srshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqrshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_ushl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nushl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_urshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqrshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuqrshl v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD integer add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_add_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nadd v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_addp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\naddp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsub v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD halving add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_shadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_srhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsrhadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_shsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_shsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nshsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_urhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nurhadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uhsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uhsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuhsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD min/max/absdiff vector ---------- */

ARM64_HW_TEMPLATE void simd_smax_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smax_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmax v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_smin_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smin_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmin v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sabd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sabd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsabd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_umax_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umax_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numax v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_umin_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umin_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numin v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uabd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uabd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nuabd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD multiply/pairwise vector ---------- */

ARM64_HW_TEMPLATE void simd_mul_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mul_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mul_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mul_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mul_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_mul_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nmul v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_smaxp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsmaxp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sminp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsminp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_umaxp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numaxp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uminp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\numinp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD special multiply vector ---------- */

ARM64_HW_TEMPLATE void simd_sqdmulh_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqdmulh_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqdmulh_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqdmulh_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqdmulh v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqrdmulh_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmulh_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmulh_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmulh_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nsqrdmulh v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_pmul_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\npmul v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_pmul_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\npmul v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD dot/matrix accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.2s, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_usdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.2s, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_usdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.2s, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.4s, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_udot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.2s, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_udot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsmmla v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_usmmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusmmla v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfmmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmmla v0.4s, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ummla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nummla v0.4s, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD saturating scalar ---------- */

ARM64_HW_TEMPLATE void simd_sqadd_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqadd b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqadd h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqadd s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqadd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqadd d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqsub_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqsub b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqsub h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqsub s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqsub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqsub d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqshl b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqshl h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqshl s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqshl d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqrshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrshl b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrshl h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrshl s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrshl d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqadd_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqadd b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqadd h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqadd s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqadd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqadd d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqsub_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqsub b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqsub h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqsub s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqsub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqsub d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqshl b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqshl h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqshl s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqshl d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uqrshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqrshl b0, b1, b2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqrshl h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqrshl s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqrshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nuqrshl d0, d1, d2\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD integer scalar ---------- */

ARM64_HW_TEMPLATE void simd_sqdmulh_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqdmulh h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqdmulh_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqdmulh s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmgt d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmge d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsshl d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_srshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsrshl d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_add_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nadd d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmtst_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmtst d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmulh_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmulh h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqrdmulh_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsqrdmulh s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhi_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmhi d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmhs_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmhs d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_ushl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nushl d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_urshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nurshl d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsub d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_cmeq_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\ncmeq d0, d1, d2\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP scalar ---------- */

ARM64_HW_TEMPLATE void simd_fmulx_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmeq_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmeq h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmeq s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmeq d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_frecps_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrecps h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrecps s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrecps d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_frsqrts_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrsqrts h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrsqrts s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfrsqrts d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmge_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmge h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmge s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmge d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_facge_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacge h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacge s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacge d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fabd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfabd h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfabd s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfabd d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmgt_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmgt h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmgt s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmgt d0, d1, d2\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_facgt_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacgt h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacgt s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfacgt d0, d1, d2\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD indexed dot accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sudot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sudot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.2s, v1.4h, v2.2h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfdot v0.4s, v1.8h, v2.2h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nsdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_usdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_usdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nusdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_udot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_udot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP widening/complex/indexed ---------- */

ARM64_HW_TEMPLATE void simd_fmlal_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.2s, v1.2h, v2.2h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.4s, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.2s, v1.2h, v2.2h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.4s, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.2s, v1.2h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal v0.4s, v1.4h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.2s, v1.2h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl v0.4s, v1.4h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal2_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.2s, v1.2h, v2.2h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal2_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.4s, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl2_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.2s, v1.2h, v2.2h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl2_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.4s, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal2_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.2s, v1.2h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlal2_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlal2 v0.4s, v1.4h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl2_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.2s, v1.2h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmlsl2_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmlsl2 v0.4s, v1.4h, v2.h[0]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.4h, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2s, v1.2s, v2.2s, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.8h, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.4s, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.2d, v1.2d, v2.2d, #270\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4h, v1.4h, v2.h[0], #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.8h, v1.8h, v2.h[0], #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #180\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfcmla v0.4s, v1.4s, v2.s[0], #270\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcadd_2h_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4h, v1.4h, v2.4h, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_2h_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4h, v1.4h, v2.4h, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_2s_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2s, v1.2s, v2.2s, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_2s_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2s, v1.2s, v2.2s, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_4h_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.8h, v1.8h, v2.8h, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_4h_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.8h, v1.8h, v2.8h, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_4s_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4s, v1.4s, v2.4s, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_4s_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.4s, v1.4s, v2.4s, #270\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_2d_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2d, v1.2d, v2.2d, #90\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcadd_2d_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcadd v0.2d, v1.2d, v2.2d, #270\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmla_2h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_4h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_2d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_2h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_4h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_2d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla h0, h1, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla s0, s1, v2.s[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmla_d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmla d0, d1, v2.d[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls h0, h1, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls s0, s1, v2.s[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmls_d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmls d0, d1, v2.d[0]\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmul_2h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_2s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_4h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_4s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_2d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_2h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_2s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_4h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_4s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_2d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmul h0, h1, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul s0, s1, v2.s[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul d0, d1, v2.d[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nfmulx h0, h1, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx s0, s1, v2.s[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx d0, d1, v2.d[0]\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD BFMLAL accumulate ---------- */

ARM64_HW_TEMPLATE void simd_bfmlalb_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalb v0.4s, v1.8h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfmlalb_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalb v0.4s, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfmlalt_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalt v0.4s, v1.8h, v2.h[0]\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bfmlalt_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbfmlalt v0.4s, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD reverse ---------- */

ARM64_HW_TEMPLATE void simd_rev64_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.8b, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev64_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.4h, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev64_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev64_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.16b, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev64_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.8h, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev64_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev64 v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev16_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev16 v0.8b, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev16_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev16 v0.16b, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev32_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev32 v0.8b, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev32_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev32 v0.4h, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev32_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev32 v0.16b, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_rev32_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nrev32 v0.8h, v1.8h\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD integer reduce ---------- */

ARM64_HW_TEMPLATE void simd_saddlv_h_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsaddlv h0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saddlv_s_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsaddlv s0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saddlv_h_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsaddlv h0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saddlv_s_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsaddlv s0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_saddlv_d_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsaddlv d0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_smaxv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsmaxv b0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsmaxv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsmaxv b0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsmaxv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_smaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsmaxv s0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sminv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsminv b0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsminv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsminv b0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsminv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsminv s0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_addv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\naddv b0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\naddv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\naddv b0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\naddv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_addv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\naddv s0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uaddlv_h_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuaddlv h0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaddlv_s_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuaddlv s0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaddlv_h_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuaddlv h0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaddlv_s_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuaddlv s0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uaddlv_d_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuaddlv d0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_umaxv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numaxv b0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numaxv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numaxv b0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numaxv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_umaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numaxv s0, v1.4s\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_uminv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numinv b0, v1.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numinv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numinv b0, v1.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numinv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\numinv s0, v1.4s\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD narrow ---------- */

ARM64_HW_TEMPLATE void simd_xtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nxtn v0.8b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_xtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nxtn v0.4h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_xtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nxtn v0.2s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.8b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.4h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtn v0.2s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.8b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.4h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nsqxtun v0.2s, v1.2d\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.8b, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.4h, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nuqxtn v0.2s, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_sqxtn_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn b0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn h0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtn_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtn s0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun b0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun h0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_sqxtun_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nsqxtun s0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn b0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn h0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_uqxtn_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nuqxtn s0, d1\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP special vector ---------- */

ARM64_HW_TEMPLATE void simd_famax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x0EC21C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x0EA2DC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EC21C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EA2DC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famax_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x4EE2DC20\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_famin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EC21C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EA2DC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EC21C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EA2DC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_famin_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EE2DC20\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fscale_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EC23C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fscale_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x2EA2FC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fscale_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EC23C20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fscale_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EA2FC20\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fscale_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\n.inst 0x6EE2FC20\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP unary vector ---------- */

ARM64_HW_TEMPLATE void simd_fabs_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfabs v0.4h, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfabs v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frintm_2s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabs_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfabs v0.8h, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfabs v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfabs v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fneg_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfneg v0.4h, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fneg_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfneg v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fneg_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfneg v0.8h, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fneg_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfneg v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fneg_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfneg v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fsqrt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.4h, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsqrt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.2s, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsqrt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.8h, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsqrt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.4s, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsqrt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfsqrt v0.2d, v1.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP binary vector ---------- */

ARM64_HW_TEMPLATE void simd_fmaxnm_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnm_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnm_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnm_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnm_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmulx_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmulx_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmeq_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmeq v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmax_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_frecps_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frecps_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrecps v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fminnm_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnm_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnm_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnm_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnm_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmin_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_frsqrts_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_frsqrts_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfrsqrts v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmaxnmp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnmp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_faddp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfaddp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmul_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmul_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmge v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_facge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacge v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fmaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fdiv_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fdiv_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fdiv_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fdiv_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fdiv_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fminnmp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnmp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fabd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfabd v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfcmgt v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_facgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_facgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfacgt v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.4h, v1.4h, v2.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.2s, v1.2s, v2.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.8h, v1.8h, v2.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.4s, v1.4s, v2.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminp v0.2d, v1.2d, v2.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP compare zero ---------- */

ARM64_HW_TEMPLATE void simd_fcmgt_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmgt h0, h1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmgt s0, s1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmgt d0, d1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.4h, v1.4h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.2s, v1.2s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.8h, v1.8h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.4s, v1.4s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmgt_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmgt v0.2d, v1.2d, #0.0\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmeq_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmeq h0, h1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmeq s0, s1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmeq d0, d1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.4h, v1.4h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.2s, v1.2s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.8h, v1.8h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.4s, v1.4s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmeq_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmeq v0.2d, v1.2d, #0.0\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmlt_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmlt h0, h1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmlt s0, s1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmlt d0, d1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.4h, v1.4h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.2s, v1.2s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.8h, v1.8h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.4s, v1.4s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmlt_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmlt v0.2d, v1.2d, #0.0\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmge_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmge h0, h1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmge s0, s1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmge d0, d1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmge v0.4h, v1.4h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmge v0.2s, v1.2s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmge v0.8h, v1.8h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmge v0.4s, v1.4s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmge_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmge v0.2d, v1.2d, #0.0\nstr q0, [x5]\nret\n"); }

ARM64_HW_TEMPLATE void simd_fcmle_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmle h0, h1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmle s0, s1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfcmle d0, d1, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmle v0.4h, v1.4h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmle v0.2s, v1.2s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmle v0.8h, v1.8h, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmle v0.4s, v1.4s, #0.0\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fcmle_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfcmle v0.2d, v1.2d, #0.0\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD FP reduce ---------- */

ARM64_HW_TEMPLATE void simd_fmaxnmv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxnmv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxnmv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxnmv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxnmv s0, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fmaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmaxv s0, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminnmv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminnmv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminnmv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminnmv s0, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminv h0, v1.4h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminv h0, v1.8h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_fminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfminv s0, v1.4s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_h_2h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfaddp h0, v1.2h\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_s_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfaddp s0, v1.2s\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_faddp_d_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfaddp d0, v1.2d\nstr q0, [x5]\nret\n"); }

/* ---------- SIMD logical ---------- */
ARM64_HW_TEMPLATE void simd_and_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nand v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_and_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nand v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bic_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bic_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_orr_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norr v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_orr_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norr v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_orn_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norn v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_orn_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\norn v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_eor_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\neor v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_eor_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\neor v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bsl_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbsl v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bsl_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbsl v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bit_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbit v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bit_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbit v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bif_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbif v0.8b, v1.8b, v2.8b\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void simd_bif_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nldr q2, [x2]\nbif v0.16b, v1.16b, v2.16b\nstr q0, [x5]\nret\n"); }

/* ---------- Scalar FP binary ---------- */

ARM64_HW_TEMPLATE void fp_fmul_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmul_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmul_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmul d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fdiv_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fdiv_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fdiv_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfdiv d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfadd d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfsub d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmax_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmax_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmax d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmin_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmin_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmin d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmaxnm_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmaxnm_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmaxnm_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfmaxnm d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fminnm_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fminnm_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fminnm_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfminnm d0, d1, d2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmul_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul h0, h1, h2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmul_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul s0, s1, s2\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmul_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nfnmul d0, d1, d2\nstr q0, [x5]\nret\n"); }

/* ---------- Scalar FP unary ---------- */

ARM64_HW_TEMPLATE void fp_fmov_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmov h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmov_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmov s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmov_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nfmov d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fabs_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fabs_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fabs_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfabs d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fneg_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fneg_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fneg_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfneg d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsqrt_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsqrt_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fsqrt_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfsqrt d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintn_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintn_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintn_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintn d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintp_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintp_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintp_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintp d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintm_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintm_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintm_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintm d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintz_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintz_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintz_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintz d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinta_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinta_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinta_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinta d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintx_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintx_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frintx_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrintx d0, d1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinti_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti h0, h1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinti_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti s0, s1\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_frinti_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q0, [x0]\nldr q1, [x1]\nfrinti d0, d1\nstr q0, [x5]\nret\n"); }

/* ---------- Scalar FP ternary ---------- */

ARM64_HW_TEMPLATE void fp_fmadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd h0, h1, h2, h3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd s0, s1, s2, s3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmadd d0, d1, d2, d3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub h0, h1, h2, h3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub s0, s1, s2, s3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fmsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfmsub d0, d1, d2, d3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd h0, h1, h2, h3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd s0, s1, s2, s3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmadd d0, d1, d2, d3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub h0, h1, h2, h3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub s0, s1, s2, s3\nstr q0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fnmsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x1]\nldr q2, [x2]\nldr q3, [x3]\nfnmsub d0, d1, d2, d3\nstr q0, [x5]\nret\n"); }

/* ---------- Scalar FP compare ---------- */

ARM64_HW_TEMPLATE void fp_fcmp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp h1, h2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp s1, s2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmp d1, d2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe h1, h2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe s1, s2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nldr q2, [x1]\nfcmpe d1, d2\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmp_zero_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmp h1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmp_zero_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmp s1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmp_zero_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmp d1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_zero_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmpe h1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_zero_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmpe s1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void fp_fcmpe_zero_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ldr q1, [x0]\nfcmpe d1, #0.0\nmrs x0, nzcv\nstr x0, [x5]\nret\n"); }

/* ======================== 数据处理立即数指令模板 ======================== */

/* ---------- move wide ---------- */

ARM64_HW_TEMPLATE void movn_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv w0, w1, w2\nmvn w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void movn_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x0, x1, x2\nmvn x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void movz_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv w0, w1, w2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void movz_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x0, x1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void movk_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv w1, w1, w2\nmov w3, #0xffff\nlslv w3, w3, w2\nbic w0, w0, w3\norr w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void movk_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x1, x1, x2\nmov x3, #0xffff\nlslv x3, x3, x2\nbic x0, x0, x3\norr x0, x0, x1\nstr x0, [x5]\nret\n"); }

/* ---------- add/sub with carry ---------- */

ARM64_HW_TEMPLATE void adc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nadc w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void adc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nadc x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void adcs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nadcs w0, w0, w1\nmrs x4, nzcv\nstr x4, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void adcs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nadcs x0, x0, x1\nmrs x4, nzcv\nstr x4, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nsbc w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nsbc x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbcs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nsbcs w0, w0, w1\nmrs x4, nzcv\nstr x4, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbcs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msr nzcv, x2\nsbcs x0, x0, x1\nmrs x4, nzcv\nstr x4, [x5, #8]\nstr x0, [x5]\nret\n"); }

/* ---------- bit operations and population count ---------- */

ARM64_HW_TEMPLATE void rbit_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rbit w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rbit_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rbit x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rev16_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rev16 w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rev16_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rev16 x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rev32_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rev w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rev32_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rev32 x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rev64_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rev x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void clz_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clz w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void clz_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("clz x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cls_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cls w0, w0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cls_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cls x0, x0\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cnt_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("movi v0.2d, #0\nfmov s0, w0\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov w0, v0.b[0]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void cnt_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("movi v0.2d, #0\nfmov d0, x0\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov w0, v0.b[0]\nstr x0, [x5]\nret\n"); }

/* ---------- multiply-add and absolute value ---------- */

ARM64_HW_TEMPLATE void madd_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("madd w0, w0, w1, w2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void madd_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("madd x0, x0, x1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void msub_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msub w0, w0, w1, w2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void msub_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("msub x0, x0, x1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void abs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w0, #0\ncneg w0, w0, mi\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void abs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp x0, #0\ncneg x0, x0, mi\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smaddl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("smaddl x0, w0, w1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smsubl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("smsubl x0, w0, w1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smulh(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("smulh x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umaddl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("umaddl x0, w0, w1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umsubl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("umsubl x0, w0, w1, x2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umulh(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("umulh x0, x0, x1\nstr x0, [x5]\nret\n"); }

/* ---------- ADD/SUB：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void add_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void add_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("add x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void adds_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("adds w0, w0, w1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void adds_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("adds x0, x0, x1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sub_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sub w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sub_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sub x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void subs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("subs w0, w0, w1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void subs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("subs x0, x0, x1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }

/* ---------- 逻辑运算：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void and_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("and w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void and_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("and x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bic w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bic x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void orr_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("orr w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void orr_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("orr x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void orn_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("orn w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void orn_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("orn x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void eor_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("eor w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void eor_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("eor x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void eon_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("eon w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void eon_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("eon x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ands_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ands w0, w0, w1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ands_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("ands x0, x0, x1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bics_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bics w0, w0, w1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bics_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bics x0, x0, x1\nmrs x3, nzcv\nstr x3, [x5, #8]\nstr x0, [x5]\nret\n"); }

/* ---------- 除法、可变移位、旋转和 CRC ---------- */

ARM64_HW_TEMPLATE void udiv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("udiv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void udiv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("udiv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sdiv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sdiv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sdiv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("sdiv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lslv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lslv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lslv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lsrv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsrv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void lsrv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsrv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void asrv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("asrv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void asrv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("asrv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rorv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rorv w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void rorv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rorv x0, x0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32b w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32h w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32w w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32x w0, w0, x1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32cb(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32cb w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32ch(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32ch w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32cw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32cw w0, w0, w1\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void crc32cx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("crc32cx w0, w0, x1\nstr x0, [x5]\nret\n"); }

/* ---------- MIN/MAX：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void smax_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, gt\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smax_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, gt\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umax_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, hi\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umax_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, hi\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smin_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, lt\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void smin_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, lt\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umin_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp w0, w1\ncsel w0, w0, w1, lo\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void umin_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("cmp x0, x1\ncsel x0, x0, x1, lo\nstr x0, [x5]\nret\n"); }

/* ---------- EXTR 与位域 ---------- */

ARM64_HW_TEMPLATE void extract_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("neg w3, w2\nlslv w3, w0, w3\nlsrv w0, w1, w2\ncmp w2, #0\ncsel w3, wzr, w3, eq\norr w0, w0, w3\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void extract_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("neg x3, x2\nlslv x3, x0, x3\nlsrv x0, x1, x2\ncmp x2, #0\ncsel x3, xzr, x3, eq\norr x0, x0, x3\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsrv w6, w0, w4\nand w6, w6, #1\nneg w6, w6\nbic w6, w6, w2\nrorv w0, w0, w3\nand w0, w0, w1\nand w0, w0, w2\norr w0, w0, w6\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void sbfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("lsrv x6, x0, x4\nand x6, x6, #1\nneg x6, x6\nbic x6, x6, x2\nrorv x0, x0, x3\nand x0, x0, x1\nand x0, x0, x2\norr x0, x0, x6\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bic w6, w0, w3\nbic w0, w0, w2\nrorv w1, w1, w4\nand w1, w1, w2\norr w0, w0, w1\nand w0, w0, w3\norr w0, w0, w6\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void bfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("bic x6, x0, x3\nbic x0, x0, x2\nrorv x1, x1, x4\nand x1, x1, x2\norr x0, x0, x1\nand x0, x0, x3\norr x0, x0, x6\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ubfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rorv w0, w0, w3\nand w0, w0, w1\nand w0, w0, w2\nstr x0, [x5]\nret\n"); }
ARM64_HW_TEMPLATE void ubfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{ asm volatile("rorv x0, x0, x3\nand x0, x0, x1\nand x0, x0, x2\nstr x0, [x5]\nret\n"); }
/* ---------- 固定立即数位域模板 ---------- */
// clang-format on

#undef ARM64_HW_TEMPLATE

#endif