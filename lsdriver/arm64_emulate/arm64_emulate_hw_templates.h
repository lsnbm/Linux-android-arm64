#ifndef ARM64_EMULATE_HW_TEMPLATES_H
#define ARM64_EMULATE_HW_TEMPLATES_H

#include <linux/types.h>

#define ARM64_HW_TEMPLATE static __attribute__((__always_inline__))

/*
快速解释硬件汇编模板执行模型：

1. executor leaf 根据原机器码中的 Rd/Rn/Rm/Ra，从 pt_regs 或 fp_regs 读取
	对应的软件寄存器现场。原指令的寄存器编号只用于选择软件现场槽位，不直接
	决定模板使用的硬件寄存器编号。

2. 所有模板保留 input0-input4 和 output 六参数的 void 接口，直接调用时
	强制内联，不依赖 x0-x5 传参 ABI。未使用的参数仍由调用方传 0。
	GPR 输入和结果通过扩展汇编操作数约束分配寄存器；多指令块中提前写入的
	输出使用 early-clobber，防止覆盖尚未读取的输入或地址。
	output 只指向 executor 的临时输出区，布局与原模板一致；寄存器输出直接
	绑定结果槽，允许编译器消除临时存取，不直接提交 pt_regs 或 fp_regs。
	CASP 保留固定的连续寄存器对并显式声明 clobber；FP/Advanced SIMD 借用
	v0-v3，并声明对应向量寄存器及内存副作用，.inst 的固定寄存器编码不变。

3. 模板不会自动把被修改的参数寄存器同步到软件现场。所有架构结果均通过
	固定输出指针写入临时结果槽，再由 leaf 使用 write_gpr_or_zr()、write_gpr_or_sp()、
	write_nzcv() 等显式提交到 pt_regs。FP/Advanced SIMD 模板同样先写入
	临时结果槽，不直接访问 fp_regs。

4. 外层异常处理在一批模拟开始前将真实 Q0-Q31、FPCR 和 FPSR 快照到 fp_regs，
	批处理结束后再把完整软件现场写回 CPU。模板可以把 v0-v3 当作临时寄存器，
	不依赖调用前这些硬件寄存器中保存的架构值。

5. load/store、原子、屏障等通过 memory clobber 描述内存副作用，硬件访问
	宽度和顺序指令保持不变。分支、PC 更新及系统语义由 executor leaf
	修改软件现场，不强行执行原始硬件指令。编码中的 immediate、lane、rotation
	等不能由参数寄存器替换的字段仍对应独立的固定模板。

模板统一以 ARM64_HW_TEMPLATE 声明，宏使用 static 和强制内联属性，禁止 naked
和汇编内的 ret。NZCV 修改必须声明 cc，不能隐藏函数调用或省略被修改寄存器。
PRFM 地址包装通过类型明确的 C 调用派发目标，编译器据此处理调用边界。
取地址并通过函数指针调用的模板仍可能生成独立函数，不能保证此类调用被内联。

Makefile 全局启用模板显式汇编需要的 ISA 扩展，并禁止普通 C 自动向量化；
最低内核版本的编译工具链 Clang 12 无法识别的助记符继续使用 .inst。但是cpu支持直接写.inst

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
{
	asm volatile("clrex #0"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #1"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #2"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #3"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #4"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #5"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #6"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #7"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #8"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #9"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #10"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #11"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #12"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #13"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #14"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void clrex_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clrex #15"
			 :
			 :
			 : "memory");
}

/* ---------- DSB ---------- */

ARM64_HW_TEMPLATE void dsb_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #0"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #1"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #2"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #3"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #4"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #5"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #6"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #7"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #8"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #9"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #10"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #11"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #12"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #13"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #14"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dsb_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dsb #15"
			 :
			 :
			 : "memory");
}

/* ---------- DMB ---------- */

ARM64_HW_TEMPLATE void dmb_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #0"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #1"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #2"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #3"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #4"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #5"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #6"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #7"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #8"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #9"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #10"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #11"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #12"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #13"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #14"
			 :
			 :
			 : "memory");
}
ARM64_HW_TEMPLATE void dmb_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dmb #15"
			 :
			 :
			 : "memory");
}

/* ---------- ISB ---------- */

ARM64_HW_TEMPLATE void isb_hw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("isb" : : : "memory");
}

ARM64_HW_TEMPLATE void dc_zva_hw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dc zva, %[input0]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}

/* ---------- YIELD ---------- */

ARM64_HW_TEMPLATE void yield_hw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("yield"
			 :
			 : );
}

/* ======================== 访存指令模板 ======================== */

/* ---------- GPR load ---------- */

ARM64_HW_TEMPLATE void ldrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsw %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldr_literal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr %w[result0], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_literal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr %[result0], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_literal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldrsw %[result0], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldurb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldurh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldursb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldursb %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldursh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldursh %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldursw %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}

/* ---------- unprivileged GPR load ---------- */

ARM64_HW_TEMPLATE void ldtrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtr %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtr %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrsb %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrsb %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrsh %w[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrsh %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldtrsw %[result0], [%[input0]]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void sturb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sturb %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void sturh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sturh %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stur %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stur %[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void sttrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sttrb %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sttrh %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sttr %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sttr %[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void strb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("strb %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void strh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("strh %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("str %w[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("str %[input1], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

/* ---------- GPR hardware address generation ---------- */

ARM64_HW_TEMPLATE void ldr_addr_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldr_addr_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrb %w[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrh %w[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldr %w[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldr %[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldr_addr_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrb %w[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldur_addr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldur b0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldur h0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldur s0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldur d0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldur q0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void ld1_addr_simd_element_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ld1_addr_simd_element_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ld1_addr_simd_element_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ld1_addr_simd_element_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void stur_addr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstur b0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_addr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstur h0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_addr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstur s0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_addr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstur d0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_addr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstur q0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrh %w[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %w[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldrsb_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldrsb_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrsb %w[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrsb %[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrsh %w[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrsh %[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result1], %[input0], %[input1]\nldrsw %[result0], [%[result1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldrsb_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %w[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %w[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsw %[result0], [%[input0]]\nadd %[result1], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void str_addr_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstrb %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstrh %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstr %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstr %[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void str_addr_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstrb %w[input2], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstrh %w[input2], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstr %w[input2], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstr %[input2], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void str_addr_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("strb %w[input2], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("strh %w[input2], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("str %w[input2], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_addr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("str %[input2], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldurb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldurh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldur %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldur %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursb_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldursb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursb_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldursb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursh_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldursh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursh_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldursh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldursw_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldursw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void stur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsturb %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsturh %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstur %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstur %[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldtr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsb_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsb_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsh_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsh_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldtrsw_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldtrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void sttr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsttrb %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsttrh %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsttr %w[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void sttr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nsttr %[input2], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldr_reg_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldrsb_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsb_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsh_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldrsw_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work1], %[input0], %[work1]\nldrsw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void str_reg_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrb %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrh %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_w_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_x_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrb %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrh %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_w_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_x_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrb %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrh %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_w_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_x_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrb %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstrh %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_w_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %w[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void str_reg_x_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input3]\nadd %[work1], %[input0], %[work1]\nstr %[input2], [%[work1]]"
			 : [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}

ARM64_HW_TEMPLATE void add_addr_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void add_addr_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void add_addr_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void add_addr_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void add_addr_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}

/* ---------- conditional select ---------- */

ARM64_HW_TEMPLATE void csel_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsel %w[result0], %w[input0], %w[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csel_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsel %[result0], %[input0], %[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csinc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsinc %w[result0], %w[input0], %w[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csinc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsinc %[result0], %[input0], %[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csinv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsinv %w[result0], %w[input0], %w[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csinv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsinv %[result0], %[input0], %[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csneg_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsneg %w[result0], %w[input0], %w[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void csneg_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input2], #0\ncsneg %[result0], %[input0], %[input1], ne"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}

/* ---------- register extend and shift ---------- */

ARM64_HW_TEMPLATE void uxtb_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtb %w[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void uxth_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxth %w[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void uxtw_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %w[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void uxtx_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sxtb_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtb %[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sxth_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxth %[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sxtw_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[result0], %w[input0]\nlslv %[result0], %[result0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sxtx_shift(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}

/* ---------- PRFM ---------- */

ARM64_HW_TEMPLATE void prfm_addr_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]"
			 : [result0] "=r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1));
	((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, void *))(uintptr_t)input4)(input0, 0, 0, 0, 0, output);
}
ARM64_HW_TEMPLATE void prfm_addr_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(input0), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
	((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, void *))(uintptr_t)input4)(input0, 0, 0, 0, 0, output);
}
ARM64_HW_TEMPLATE void prfm_addr_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(input0), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
	((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, void *))(uintptr_t)input4)(input0, 0, 0, 0, 0, output);
}
ARM64_HW_TEMPLATE void prfm_addr_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(input0), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
	((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, void *))(uintptr_t)input4)(input0, 0, 0, 0, 0, output);
}
ARM64_HW_TEMPLATE void prfm_addr_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[result0], %[input0], %[work1]"
			 : [result0] "=&r"(input0), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
	((void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, void *))(uintptr_t)input4)(input0, 0, 0, 0, 0, output);
}

ARM64_HW_TEMPLATE void prfm_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #0, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #1, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #2, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #3, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #4, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #5, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #6, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #7, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #8, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #9, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #10, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #11, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #12, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #13, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #14, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #15, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_16(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #16, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_17(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #17, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_18(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #18, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_19(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #19, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_20(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #20, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_21(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #21, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_22(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #22, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_23(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #23, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_24(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #24, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_25(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #25, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_26(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #26, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_27(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #27, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_28(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #28, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_29(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #29, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_30(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #30, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void prfm_31(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("prfm #31, [%[input0]]"
			 :
			 : [input0] "r"(input0)
			 : "memory");
}

/* ---------- RCpc ---------- */

ARM64_HW_TEMPLATE void ldapur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapurb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapurh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapur %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapur %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_sb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapursb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_sb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapursb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_sh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapursh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_sh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapursh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_addr_sw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapursw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlurb %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlurh %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlur %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlur %[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldaprb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldaprh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldapr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldapurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapurb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapurh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapur %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapur %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapursb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapursb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapursb_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapursb %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapursh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapursh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapursh_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapursh %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapursw_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapursw %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void stlurb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[result0], %[input1]\nmov %[work1], %[work2]\nstlurb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlurh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[result0], %[input1]\nmov %[work1], %[work2]\nstlurh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[result0], %[input1]\nmov %[work1], %[work2]\nstlur %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlur_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[result0], %[input1]\nmov %[work1], %[work2]\nstlur %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldaprb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaprb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaprh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaprh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldapr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldapr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}

/* ---------- ordered ---------- */

ARM64_HW_TEMPLATE void ldlar_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldlarb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlar_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldlarh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlar_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldlar %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlar_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldlar %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldarb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldarh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldar %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work1], %[input0], %[input1]\nldar %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstllrb %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstllrh %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstllr %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstllr %[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlrb %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlrh %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlr %w[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nstlr %[input2], [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldlarb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldlarb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlarh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldlarh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlar_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldlar %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldlar_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldlar %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldarb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldarb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldarh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldarh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldar %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldar_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldar %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void stllrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstllrb %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstllrh %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstllr %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stllr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstllr %[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstlrb %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstlrh %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstlr %w[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nmov %[work0], %[input1]\nmov %[work1], %[work2]\nstlr %[work0], [%[work1]]"
			 : [work0] "=&r"(input0), [work1] "=&r"(input1), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

/* ---------- exclusive ---------- */

ARM64_HW_TEMPLATE void ldxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldxrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaxrb %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldxrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaxrh %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldxr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaxr %w[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldxr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work1], %[input0]\nldaxr %[result0], [%[work1]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldxp %w[result0], %w[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldaxp %w[result0], %w[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldxp %[result0], %[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldaxp %[result0], %[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void stxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstxrb %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxrb_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstlxrb %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstxrh %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxrh_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstlxrh %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstxr %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstlxr %w[result0], %w[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstxr %w[result0], %[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nstlxr %w[result0], %[input1], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nstxp %w[result0], %w[input1], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nstlxp %w[result0], %w[input1], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nstxp %w[result0], %[input1], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nstlxp %w[result0], %[input1], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldxrb %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldxrh %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldxr %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldxr %[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldaxrb %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldaxrh %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldaxr %w[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nldaxr %[result0], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldxp %w[result0], %w[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldxp %[result0], %[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldaxp %w[result0], %w[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldaxp %[result0], %[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxrb %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxrh %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxr %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxr %w[result0], %[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxrb %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxrh %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxr %w[result0], %w[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxr %w[result0], %[input2], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxp %w[result0], %w[input2], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstxp %w[result0], %[input2], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxp %w[result0], %w[input2], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stlxp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nstlxp %w[result0], %[input2], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}

/* ---------- LSE RMW ---------- */

ARM64_HW_TEMPLATE void ldadd_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldadd %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldadd %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddab %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddah %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldadda %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldadda %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddlb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddlh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddl %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddl %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddalb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddalh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddal %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldaddal %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldclr_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclr %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclr %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrab %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrah %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclra %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclra %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrlb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrlh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrl %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclrl %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclralb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclralh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclral %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldclral %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldeor_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeor %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeor %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorab %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorah %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeora %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeora %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorlb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorlh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorl %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeorl %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeoralb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeoralh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeoral %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldeoral %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldset_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldseth %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldset %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldset %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetab %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetah %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldseta %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldseta %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetlb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetlh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetl %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetl %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetalb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetalh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetal %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nldsetal %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void swp_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswph %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswp %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswp %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpab %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpah %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpa %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpa %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswplb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswplh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpl %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpl %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpalb %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpalh %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpal %w[input2], %w[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work3], %[input0], %[input1]\nswpal %[input2], %[result0], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldadd_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldadd %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadd_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldadd %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldadda %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldadda_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldadda %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldaddal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldaddal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldclr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclr %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclr %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclra %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclra_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclra %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclrl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclrl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclralb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclralh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclral %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldclral_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldclral %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldeor_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeor %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeor_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeor %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeora %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeora_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeora %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeorl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeorl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeoralb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeoralh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeoral %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldeoral_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldeoral %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldset_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldseth %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldset %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldset_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldset %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldseta %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldseta_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldseta %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsetal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsetal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldsmax_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmax_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmax %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmax_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmax %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxa %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxa %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmaxal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmaxal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldsmin_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmin_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmin %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmin_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmin %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmina_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmina_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmina_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmina %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsmina_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsmina %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldsminal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldsminal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldumax_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumax_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumax %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumax_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumax %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxa %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxa %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumaxal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumaxal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldumin_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumin_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumin %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumin_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumin %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumina_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumina_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumina_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumina %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldumina_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nldumina %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminlb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminlh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void lduminal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nlduminal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void swp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswph %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswp %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswp %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpab %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpah %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpa %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpa %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswplb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswplh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpl %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpl %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpalb %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpalh %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpal %w[input1], %w[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void swpal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work2], %[input0]\nswpal %[input1], %[result0], [%[work2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

/* ---------- CAS ---------- */

ARM64_HW_TEMPLATE void cas_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasb %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncash %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncas %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %[result0], %[input1]\ncas %[result0], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasab %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasah %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasa %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %[result0], %[input1]\ncasa %[result0], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncaslb %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncaslh %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasl %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %[result0], %[input1]\ncasl %[result0], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasalb %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasalh %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %w[result0], %w[input1]\ncasal %w[result0], %w[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov %[work3], %[input0]\nmov %[result0], %[input1]\ncasal %[result0], %[input2], [%[work3]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

/* ---------- CASP ---------- */

ARM64_HW_TEMPLATE void casp_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncasp w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void casp_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncasp x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspa_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspa w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspa_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspa x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspl_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspl w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspl_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspl x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspal_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov w0, w1\nmov w1, w2\nmov w2, w3\nmov w3, w4\ncaspal w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspal_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x6, x0\nmov x0, x1\nmov x1, x2\nmov x2, x3\nmov x3, x4\ncaspal x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x0", "x1", "x2", "x3", "x4", "x6", "memory");
}

ARM64_HW_TEMPLATE void cas_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasb %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncash %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncas %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void cas_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %[result0], %[input2]\ncas %[result0], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasab %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasah %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasa %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %[result0], %[input2]\ncasa %[result0], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncaslb %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncaslh %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasl %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %[result0], %[input2]\ncasl %[result0], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_addr_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasalb %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_addr_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasalh %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %w[result0], %w[input2]\ncasal %w[result0], %w[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void casal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work4], %[input0], %[input1]\nmov %[result0], %[input2]\ncasal %[result0], %[input3], [%[work4]]"
			 : [result0] "=&r"(*(uint64_t *)output), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}

ARM64_HW_TEMPLATE void casp_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncasp w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void casp_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncasp x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspa_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspa w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspa_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspa x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspl_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspl w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspl_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspl x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspal_addr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov w0, w2\nmov w1, w3\nmov w2, w4\nldr w3, [x5, #16]\ncaspal w0, w1, w2, w3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}
ARM64_HW_TEMPLATE void caspal_addr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mov x0, %[input0]\nmov x1, %[input1]\nmov x2, %[input2]\nmov x3, %[input3]\nmov x4, %[input4]\nmov x5, %[input5]\nadd x6, x0, x1\nmov x0, x2\nmov x1, x3\nmov x2, x4\nldr x3, [x5, #16]\ncaspal x0, x1, x2, x3, [x6]\nmov %[result0], x0\nmov %[result1], x1"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4), [input5] "r"(output)
			 : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "memory");
}

/* ---------- FP/SIMD load ---------- */

ARM64_HW_TEMPLATE void ldur_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur b0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur h0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur s0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur d0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldur_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldur q0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void ldr_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr b0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr h0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr s0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr d0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void ldr_literal_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldr s0, [%[work0]]\nstr q0, [%[input5]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_literal_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldr d0, [%[work0]]\nstr q0, [%[input5]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_literal_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldr q0, [%[work0]]\nstr q0, [%[input5]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- FP/SIMD store ---------- */

ARM64_HW_TEMPLATE void stur_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstur b0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstur h0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstur s0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstur d0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void stur_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstur q0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void str_fp_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstr b0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_fp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstr h0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstr s0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstr d0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nstr q0, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v0", "memory");
}

/* ---------- FP/SIMD address-aware load ---------- */

ARM64_HW_TEMPLATE void ldr_addr_fp_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work2], %[input0], %[input1]\nldr b0, [%[work2]]\nstr q0, [%[input5]]"
			 : [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work2], %[input0], %[input1]\nldr h0, [%[work2]]\nstr q0, [%[input5]]"
			 : [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work2], %[input0], %[input1]\nldr s0, [%[work2]]\nstr q0, [%[input5]]"
			 : [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work2], %[input0], %[input1]\nldr d0, [%[work2]]\nstr q0, [%[input5]]"
			 : [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work2], %[input0], %[input1]\nldr q0, [%[work2]]\nstr q0, [%[input5]]"
			 : [work2] "=&r"(input2)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void ldr_addr_fp_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr b0, [%[result0]]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr h0, [%[result0]]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr s0, [%[result0]]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr d0, [%[result0]]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldr q0, [%[result0]]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void ldr_addr_fp_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr b0, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr h0, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr s0, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr d0, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- FP/SIMD address-aware store ---------- */

ARM64_HW_TEMPLATE void str_addr_fp_b_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstr b0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_h_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstr h0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstr s0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstr d0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[work3], %[input0], %[input1]\nstr q0, [%[work3]]"
			 : [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void str_addr_fp_b_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[result0], %[input0], %[input1]\nstr b0, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_h_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[result0], %[input0], %[input1]\nstr h0, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[result0], %[input0], %[input1]\nstr s0, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[result0], %[input0], %[input1]\nstr d0, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void str_addr_fp_b_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nstr b0, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_h_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nstr h0, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nstr s0, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nstr d0, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nstr q0, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "memory");
}

/* ---------- FP/SIMD register-offset load ---------- */

ARM64_HW_TEMPLATE void ldr_reg_fp_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr b0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr h0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_s_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr s0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_d_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr d0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_q_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("uxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr q0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr b0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr h0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_s_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr s0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_d_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr d0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_q_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr q0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr b0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr h0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_s_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr s0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_d_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr d0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_q_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr q0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr b0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr h0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_s_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr s0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_d_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr d0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void ldr_reg_fp_q_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsl %[work1], %[input1], %[input2]\nadd %[work3], %[input0], %[work1]\nldr q0, [%[work3]]\nstr q0, [%[input5]]"
			 : [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- FP/SIMD register-offset store ---------- */

ARM64_HW_TEMPLATE void str_reg_fp_b_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nuxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr b0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_h_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nuxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr h0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_s_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nuxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr s0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_d_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nuxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr d0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_q_uxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nuxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr q0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_b_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr b0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_h_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr h0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_s_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr s0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_d_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr d0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_q_uxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr q0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_b_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nsxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr b0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_h_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nsxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr h0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_s_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nsxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr s0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_d_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nsxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr d0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_q_sxtw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nsxtw %[work1], %w[input1]\nlsl %[work1], %[work1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr q0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_b_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr b0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_h_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr h0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_s_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr s0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_d_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr d0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void str_reg_fp_q_sxtx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input3]]\nlsl %[work1], %[input1], %[input2]\nadd %[work4], %[input0], %[work1]\nstr q0, [%[work4]]"
			 : [work1] "=&r"(input1), [work4] "=&r"(input4)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "memory");
}

/* ---------- GPR pair load/store ---------- */

ARM64_HW_TEMPLATE void ldnp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldnp %w[result0], %w[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldnp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldnp %[result0], %[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp %w[result0], %w[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp %[result0], %[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldpsw_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldpsw %[result0], %[result1], [%[input0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8))
			 : [input0] "r"(input0)
			 : "memory");
}

ARM64_HW_TEMPLATE void stnp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stnp %w[input1], %w[input2], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stnp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stnp %[input1], %[input2], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_gpr_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stp %w[input1], %w[input2], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_gpr_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stp %[input1], %[input2], [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "memory");
}

ARM64_HW_TEMPLATE void ldp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldp %w[result0], %w[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldp %[result0], %[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldnp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldnp %w[result0], %w[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldnp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldnp %[result0], %[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldpsw %[result0], %[result1], [%[work0]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_gpr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result2], %[input0], %[input1]\nldp %w[result0], %w[result1], [%[result2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result2], %[input0], %[input1]\nldp %[result0], %[result1], [%[result2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result2], %[input0], %[input1]\nldpsw %[result0], %[result1], [%[result2]]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_gpr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp %w[result0], %w[result1], [%[input0]]\nadd %[result2], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp %[result0], %[result1], [%[input0]]\nadd %[result2], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void ldpsw_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldpsw %[result0], %[result1], [%[input0]]\nadd %[result2], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output), [result1] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result2] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void stp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstp %w[input2], %w[input3], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstp %[input2], %[input3], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stnp_addr_gpr_w_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstnp %w[input2], %w[input3], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stnp_addr_gpr_x_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nstnp %[input2], %[input3], [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_addr_gpr_w_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstp %w[input2], %w[input3], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_addr_gpr_x_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nstp %[input2], %[input3], [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_addr_gpr_w_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stp %w[input2], %w[input3], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}
ARM64_HW_TEMPLATE void stp_addr_gpr_x_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("stp %[input2], %[input3], [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "memory");
}

/* ---------- FP/SIMD pair load/store ---------- */

ARM64_HW_TEMPLATE void ldnp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldnp s0, s1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldnp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldnp d0, d1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldnp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldnp q0, q1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp s0, s1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp d0, d1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp q0, q1, [%[input0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void stnp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstnp s0, s1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stnp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstnp d0, d1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stnp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstnp q0, q1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_fp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstp s0, s1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_fp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstp d0, d1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_fp_q(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input1]]\nldr q1, [%[input2]]\nstp q0, q1, [%[input0]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void ldp_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldp s0, s1, [%[work0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldp d0, d1, [%[work0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[work0], %[input0], %[input1]\nldp q0, q1, [%[work0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldp s0, s1, [%[result0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldp d0, d1, [%[result0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %[result0], %[input0], %[input1]\nldp q0, q1, [%[result0]]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp s0, s1, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp d0, d1, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void ldp_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldp q0, q1, [%[input0]]\nadd %[result0], %[input0], %[input1]\nstr q0, [%[input5]]\nstr q1, [%[input5], #16]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void stp_addr_fp_s_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[work0], %[input0], %[input1]\nstp s0, s1, [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_d_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[work0], %[input0], %[input1]\nstp d0, d1, [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_q_base_offset(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[work0], %[input0], %[input1]\nstp q0, q1, [%[work0]]"
			 : [work0] "=&r"(input0)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_s_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[result0], %[input0], %[input1]\nstp s0, s1, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_d_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[result0], %[input0], %[input1]\nstp d0, d1, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_q_pre_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nadd %[result0], %[input0], %[input1]\nstp q0, q1, [%[result0]]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_s_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nstp s0, s1, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_d_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nstp d0, d1, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void stp_addr_fp_q_post_index(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input2]]\nldr q1, [%[input3]]\nstp q0, q1, [%[input0]]\nadd %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 32))
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3)
			 : "v0", "v1", "memory");
}

/* ======================== FP 与 Advanced SIMD 指令模板 ======================== */

/* ---------- FP/AdvSIMD merge conversion ---------- */

ARM64_HW_TEMPLATE void fp_fcvt_d_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvt d0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvt_s_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvt s0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_xtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nxtn2 v0.16b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_xtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nxtn2 v0.8h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_xtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nxtn2 v0.4s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn2 v0.16b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn2 v0.8h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn2 v0.4s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun2 v0.16b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun2 v0.8h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun2 v0.4s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn2_16b_8h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn2 v0.16b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn2_8h_4s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn2 v0.8h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn2_4s_2d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn2 v0.4s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- GPR to FP ---------- */

ARM64_HW_TEMPLATE void fp_scvtf_s_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nscvtf s0, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_ucvtf_s_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nucvtf s0, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_scvtf_d_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nscvtf d0, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_ucvtf_d_w_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nucvtf d0, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_scvtf_s_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nscvtf s0, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_ucvtf_s_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nucvtf s0, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_scvtf_d_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nscvtf d0, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void fp_ucvtf_d_x_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nucvtf d0, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- FP/AdvSIMD conversion ---------- */

ARM64_HW_TEMPLATE void simd_fcvtns_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtns s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtns_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtns d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtns_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtns v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtns_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtns v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtns_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtns v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtms_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtms s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtms_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtms d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtms_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtms v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtms_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtms v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtms_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtms v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtas_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtas s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtas_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtas d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtas_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtas v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtas_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtas v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtas_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtas v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_scvtf_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nscvtf s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_scvtf_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nscvtf d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_scvtf_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nscvtf v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_scvtf_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nscvtf v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_scvtf_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nscvtf v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtps_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtps s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtps_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtps d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtps_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtps v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtps_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtps v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtps_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtps v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtzs_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtzs s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzs_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtzs d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzs v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzs v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzs v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtnu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtnu s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtnu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtnu d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtnu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtnu v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtnu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtnu v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtnu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtnu v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtmu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtmu s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtmu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtmu d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtmu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtmu v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtmu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtmu v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtmu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtmu v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtau_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtau s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtau_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtau d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtau_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtau v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtau_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtau v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtau_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtau v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_ucvtf_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nucvtf s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_ucvtf_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nucvtf d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_ucvtf_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nucvtf v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_ucvtf_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nucvtf v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_ucvtf_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nucvtf v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtpu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtpu s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtpu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtpu d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtpu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtpu v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtpu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtpu v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtpu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtpu v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcvtzu_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtzu s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzu_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcvtzu d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzu_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzu v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzu_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzu v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcvtzu_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcvtzu v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- FP to GPR ---------- */

ARM64_HW_TEMPLATE void fp_fcvtns_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtns %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtns_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtns %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtns_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtns %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtns_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtns %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtnu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtnu %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtnu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtnu %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtnu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtnu %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtnu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtnu %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtas_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtas %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtas_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtas %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtas_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtas %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtas_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtas %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtau_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtau %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtau_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtau %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtau_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtau %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtau_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtau %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtps_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtps %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtps_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtps %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtps_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtps %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtps_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtps %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtpu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtpu %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtpu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtpu %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtpu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtpu %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtpu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtpu %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtms_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtms %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtms_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtms %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtms_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtms %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtms_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtms %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtmu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtmu %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtmu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtmu %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtmu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtmu %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtmu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtmu %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzs_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzs %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzs_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzs %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzs_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzs %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzs_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzs %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzu_w_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzu %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzu_w_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzu %w[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzu_x_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzu %[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fcvtzu_x_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcvtzu %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}

/* ---------- FP select ---------- */

ARM64_HW_TEMPLATE void fp_fcsel_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmp %w[input3], #0\nfcsel h0, h1, h2, ne\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcsel_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmp %w[input3], #0\nfcsel s0, s1, s2, ne\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcsel_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmp %w[input3], #0\nfcsel d0, d1, d2, ne\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "cc", "memory");
}

/* ---------- SIMD lane and scalar transfer ---------- */

ARM64_HW_TEMPLATE void simd_extract_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrb %w[result0], [%[input0], %w[input1], uxtw]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrh %w[result0], [%[input0], %w[input1], uxtw #1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %w[result0], [%[input0], %w[input1], uxtw #2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr %[result0], [%[input0], %w[input1], uxtw #3]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_signed_b_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %w[result0], [%[input0], %w[input1], uxtw]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_signed_h_w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %w[result0], [%[input0], %w[input1], uxtw #1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_signed_b_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsb %[result0], [%[input0], %w[input1], uxtw]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_signed_h_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsh %[result0], [%[input0], %w[input1], uxtw #1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}
ARM64_HW_TEMPLATE void simd_extract_signed_s_x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldrsw %[result0], [%[input0], %w[input1], uxtw #2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "memory");
}

ARM64_HW_TEMPLATE void simd_insert_b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nstr q0, [%[input5]]\nstrb %w[input1], [%[input5], %w[input2], uxtw]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_insert_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nstr q0, [%[input5]]\nstrh %w[input1], [%[input5], %w[input2], uxtw #1]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_insert_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nstr q0, [%[input5]]\nstr %w[input1], [%[input5], %w[input2], uxtw #2]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_insert_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nstr q0, [%[input5]]\nstr %[input1], [%[input5], %w[input2], uxtw #3]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void simd_write_scalar_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("fmov s0, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_write_scalar_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("fmov d0, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_read_scalar_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfmov %w[result0], s1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_read_scalar_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfmov %[result0], d1"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "memory");
}

/* ---------- SIMD duplicate general ---------- */

ARM64_HW_TEMPLATE void simd_dup_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.8b, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.4h, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.2s, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.16b, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.8h, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.4s, %w[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_dup_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("dup v0.2d, %[input1]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- SIMD fixed immediate templates ---------- */

ARM64_HW_TEMPLATE void simd_movi_d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nmovi d0, #0000000000000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_movi_2d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("movi v0.2d, #0000000000000000\nstr q0, [%[input5]]"
			 :
			 : [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_movi_4s_imm03(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("movi v0.4s, #3\nstr q0, [%[input5]]"
			 :
			 : [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_2s_imm70(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov v0.2s, #1.00000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_2s_immf0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov v0.2s, #-1.00000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_s_imm60(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov s0, #0.50000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_s_imme0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov s0, #-0.50000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_s_imm70(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov s0, #1.00000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_s_immf0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov s0, #-1.00000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_d_imm78(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov d0, #1.50000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_fmov_d_imm00(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nfmov d0, #2.00000000\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void simd_orr_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\norr v0.4s, #18\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_orr_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\norr v0.2s, #18\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_bic_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nbic v0.4s, #18\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_bic_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nbic v0.2s, #18\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_mvni_4s_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("mvni v0.4s, #18\nstr q0, [%[input5]]"
			 :
			 : [input5] "r"(output)
			 : "v0", "memory");
}
ARM64_HW_TEMPLATE void simd_mvni_8b_imm12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nmvni v0.2s, #18\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}

/* ---------- SIMD RDM accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sqrdmlah_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlah_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlah_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlah_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlah_h_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlah_s_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlah s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqrdmlsh_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlsh_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlsh_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlsh_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlsh_h_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmlsh_s_scalar_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmlsh s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD permute vector ---------- */

ARM64_HW_TEMPLATE void simd_uzp1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp1 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_trn1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn1 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_zip1_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip1_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip1 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uzp2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uzp2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuzp2 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_trn2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_trn2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ntrn2 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_zip2_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_zip2_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nzip2 v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD integer accumulate vector ---------- */

ARM64_HW_TEMPLATE void simd_saba_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_saba_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_saba_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_saba_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_saba_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_saba_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsaba v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_mla_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mla_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mla_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mla_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mla_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmla v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uaba_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uaba_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uaba_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uaba_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uaba_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uaba_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuaba v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_mls_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mls_4h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mls_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mls_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mls_8h_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mls_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nmls v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD saturating add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_sqadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD integer compare vector ---------- */

ARM64_HW_TEMPLATE void simd_cmgt_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_cmge_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_cmtst_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_cmhi_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_cmhs_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_cmeq_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD variable shift vector ---------- */

ARM64_HW_TEMPLATE void simd_shl_4s_imm2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nshl v0.4s, v0.4s, #2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input5] "r"(output)
			 : "v0", "memory");
}

ARM64_HW_TEMPLATE void simd_sshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD extract ---------- */

ARM64_HW_TEMPLATE void simd_ext_8b_offset_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #1\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #4\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #5\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #6\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_8b_offset_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.8b, v1.8b, v2.8b, #7\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_ext_16b_offset_0(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_1(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #1\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_2(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_3(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_4(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #4\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_5(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #5\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_6(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #6\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_7(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #7\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_8(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #8\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_9(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #9\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_10(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #10\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_11(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #11\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_12(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #12\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_13(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #13\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_14(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #14\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ext_16b_offset_15(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\next v0.16b, v1.16b, v2.16b, #15\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_srshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqrshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_ushl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_urshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqrshl_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD integer add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_add_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_addp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_addp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\naddp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD halving add/sub vector ---------- */

ARM64_HW_TEMPLATE void simd_shadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_srhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrhadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_shsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_shsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nshsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_urhadd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urhadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urhadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urhadd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urhadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urhadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nurhadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uhsub_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhsub_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uhsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuhsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD min/max/absdiff vector ---------- */

ARM64_HW_TEMPLATE void simd_smax_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smax_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmax v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_smin_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smin_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmin v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sabd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sabd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsabd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_umax_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umax_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numax v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_umin_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umin_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numin v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uabd_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uabd_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nuabd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD multiply/pairwise vector ---------- */

ARM64_HW_TEMPLATE void simd_mul_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mul_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mul_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mul_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mul_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_mul_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nmul v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_smaxp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmaxp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sminp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sminp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsminp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_umaxp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numaxp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uminp_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uminp_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\numinp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD special multiply vector ---------- */

ARM64_HW_TEMPLATE void simd_sqdmulh_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqdmulh_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqdmulh_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqdmulh_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqrdmulh_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmulh_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmulh_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmulh_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_pmul_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\npmul v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_pmul_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\npmul v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD dot/matrix accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsdot v0.2s, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsdot v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_usdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nusdot v0.2s, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_usdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nusdot v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfdot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfdot v0.2s, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfdot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfdot v0.4s, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_udot_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nudot v0.2s, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_udot_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nudot v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_smmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsmmla v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_usmmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nusmmla v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfmmla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfmmla v0.4s, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ummla_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nummla v0.4s, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD saturating scalar ---------- */

ARM64_HW_TEMPLATE void simd_sqadd_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqadd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqadd d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqsub_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqsub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqsub d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_sqrshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqadd_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqadd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqadd d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqsub_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqsub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqsub d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_uqrshl_b_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl b0, b1, b2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_uqrshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nuqrshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD integer scalar ---------- */

ARM64_HW_TEMPLATE void simd_sqdmulh_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqdmulh_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqdmulh s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmgt d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmge d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_srshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsrshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_add_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nadd d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmtst_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmtst d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmulh_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sqrdmulh_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsqrdmulh s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhi_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhi d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmhs_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmhs d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_ushl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nushl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_urshl_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nurshl d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sub_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsub d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_cmeq_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\ncmeq d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD FP scalar ---------- */

ARM64_HW_TEMPLATE void simd_fmulx_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmeq_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_frecps_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_frsqrts_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmge_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_facge_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fabd_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmgt_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_facgt_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD indexed dot accumulate ---------- */

ARM64_HW_TEMPLATE void simd_sudot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sudot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfdot v0.2s, v1.4h, v2.2h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfdot v0.4s, v1.8h, v2.2h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_sdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nsdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_usdot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nusdot v0.2s, v1.8b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_usdot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nusdot v0.4s, v1.16b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_udot_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nudot v0.2s, v1.8b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_udot_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nudot v0.4s, v1.16b, v2.4b[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD FP widening/complex/indexed ---------- */

ARM64_HW_TEMPLATE void simd_fmlal_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal v0.2s, v1.2h, v2.2h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal v0.4s, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl v0.2s, v1.2h, v2.2h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl v0.4s, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal v0.2s, v1.2h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal v0.4s, v1.4h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl v0.2s, v1.2h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl v0.4s, v1.4h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal2_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal2 v0.2s, v1.2h, v2.2h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal2_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal2 v0.4s, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl2_2s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl2 v0.2s, v1.2h, v2.2h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl2_4s_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl2 v0.4s, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal2_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal2 v0.2s, v1.2h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlal2_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlal2 v0.4s, v1.4h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl2_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl2 v0.2s, v1.2h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmlsl2_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmlsl2 v0.4s, v1.4h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.4h, #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.4h, #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.4h, #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.4h, #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2s, v1.2s, v2.2s, #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2s, v1.2s, v2.2s, #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2s, v1.2s, v2.2s, #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2s_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2s, v1.2s, v2.2s, #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.8h, #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.8h, #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.8h, #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.8h, #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.4s, #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.4s, #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.4s, #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.4s, #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2d, v1.2d, v2.2d, #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2d, v1.2d, v2.2d, #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2d, v1.2d, v2.2d, #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2d_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.2d, v1.2d, v2.2d, #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.h[0], #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.h[0], #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.h[0], #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_2h_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4h, v1.4h, v2.h[0], #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.h[0], #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.h[0], #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.h[0], #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4h_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.8h, v1.8h, v2.h[0], #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_0_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.s[0], #0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_90_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.s[0], #90\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_180_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.s[0], #180\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmla_4s_indexed_rotation_270_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmla v0.4s, v1.4s, v2.s[0], #270\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcadd_2h_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.4h, v1.4h, v2.4h, #90\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_2h_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.4h, v1.4h, v2.4h, #270\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_2s_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.2s, v1.2s, v2.2s, #90\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_2s_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.2s, v1.2s, v2.2s, #270\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_4h_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.8h, v1.8h, v2.8h, #90\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_4h_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.8h, v1.8h, v2.8h, #270\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_4s_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.4s, v1.4s, v2.4s, #90\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_4s_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.4s, v1.4s, v2.4s, #270\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_2d_rotation_90(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.2d, v1.2d, v2.2d, #90\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcadd_2d_rotation_270(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcadd v0.2d, v1.2d, v2.2d, #270\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmla_2h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_4h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_2d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_2h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_2s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_4h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_4s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_2d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla h0, h1, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla s0, s1, v2.s[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmla_d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmla d0, d1, v2.d[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_h_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls h0, h1, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_s_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls s0, s1, v2.s[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmls_d_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmls d0, d1, v2.d[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmul_2h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_2s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_4h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_4s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_2d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_2h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_2s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_4h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_4s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_2d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul h0, h1, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul s0, s1, v2.s[0]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul d0, d1, v2.d[0]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_h_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx h0, h1, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_s_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx s0, s1, v2.s[0]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_d_indexed(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx d0, d1, v2.d[0]\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD BFMLAL accumulate ---------- */

ARM64_HW_TEMPLATE void simd_bfmlalb_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfmlalb v0.4s, v1.8h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfmlalb_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfmlalb v0.4s, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfmlalt_indexed_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfmlalt v0.4s, v1.8h, v2.h[0]\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bfmlalt_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbfmlalt v0.4s, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD reverse ---------- */

ARM64_HW_TEMPLATE void simd_rev64_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.8b, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev64_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.4h, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev64_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev64_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.16b, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev64_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.8h, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev64_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev64 v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev16_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev16 v0.8b, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev16_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev16 v0.16b, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev32_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev32 v0.8b, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev32_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev32 v0.4h, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev32_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev32 v0.16b, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_rev32_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nrev32 v0.8h, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD integer reduce ---------- */

ARM64_HW_TEMPLATE void simd_saddlv_h_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsaddlv h0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_saddlv_s_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsaddlv s0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_saddlv_h_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsaddlv h0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_saddlv_s_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsaddlv s0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_saddlv_d_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsaddlv d0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_smaxv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsmaxv b0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsmaxv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsmaxv b0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsmaxv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_smaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsmaxv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_sminv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsminv b0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsminv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sminv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsminv b0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsminv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsminv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_addv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\naddv b0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_addv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\naddv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_addv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\naddv b0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_addv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\naddv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_addv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\naddv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_uaddlv_h_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuaddlv h0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uaddlv_s_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuaddlv s0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uaddlv_h_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuaddlv h0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uaddlv_s_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuaddlv s0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uaddlv_d_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuaddlv d0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_umaxv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numaxv b0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numaxv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numaxv b0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numaxv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_umaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numaxv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_uminv_b_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numinv b0, v1.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numinv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uminv_b_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numinv b0, v1.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numinv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\numinv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD narrow ---------- */

ARM64_HW_TEMPLATE void simd_xtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nxtn v0.8b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_xtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nxtn v0.4h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_xtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nxtn v0.2s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtn v0.8b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtn v0.4h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtn v0.2s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtun v0.8b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtun v0.4h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nsqxtun v0.2s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_8b_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuqxtn v0.8b, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_4h_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuqxtn v0.4h, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_2s_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nuqxtn v0.2s, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_sqxtn_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn b0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn h0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtn_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtn s0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun b0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun h0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_sqxtun_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nsqxtun s0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_b_h_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn b0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_h_s_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn h0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_uqxtn_s_d_scalar(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nuqxtn s0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD FP special vector ---------- */

ARM64_HW_TEMPLATE void simd_famax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x0EC21C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x0EA2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x4EC21C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x4EA2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famax_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x4EE2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_famin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x2EC21C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x2EA2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EC21C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EA2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_famin_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EE2DC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fscale_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x2EC23C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fscale_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x2EA2FC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fscale_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EC23C20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fscale_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EA2FC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fscale_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\n.inst 0x6EE2FC20\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD FP unary vector ---------- */

ARM64_HW_TEMPLATE void simd_fabs_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfabs v0.4h, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fabs_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfabs v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_frintm_2s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintm v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fabs_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfabs v0.8h, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fabs_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfabs v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fabs_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfabs v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fneg_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfneg v0.4h, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fneg_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfneg v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fneg_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfneg v0.8h, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fneg_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfneg v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fneg_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfneg v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fsqrt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfsqrt v0.4h, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fsqrt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfsqrt v0.2s, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fsqrt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfsqrt v0.8h, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fsqrt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfsqrt v0.4s, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fsqrt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfsqrt v0.2d, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD FP binary vector ---------- */

ARM64_HW_TEMPLATE void simd_fmaxnm_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnm_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnm_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnm_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnm_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fadd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fadd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fadd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fadd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fadd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmulx_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmulx_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmeq_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmeq v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmax_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmax_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmax_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmax_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmax_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_frecps_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frecps_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrecps v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fminnm_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnm_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnm_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnm_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnm_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fsub_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fsub_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fsub_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fsub_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fsub_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmin_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmin_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmin_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmin_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmin_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_frsqrts_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_frsqrts_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfrsqrts v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmaxnmp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnmp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnmp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnmp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnmp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnmp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_faddp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfaddp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfaddp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfaddp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfaddp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfaddp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmul_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmul_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmge v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_facge_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facge_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacge v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fmaxp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fdiv_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fdiv_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fdiv_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fdiv_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fdiv_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fminnmp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnmp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnmp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnmp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnmp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnmp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fabd_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fabd_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfabd v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfcmgt v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_facgt_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_facgt_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfacgt v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

ARM64_HW_TEMPLATE void simd_fminp_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminp v0.4h, v1.4h, v2.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminp_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminp v0.2s, v1.2s, v2.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminp_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminp v0.8h, v1.8h, v2.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminp_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminp v0.4s, v1.4s, v2.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_fminp_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminp v0.2d, v1.2d, v2.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- SIMD FP compare zero ---------- */

ARM64_HW_TEMPLATE void simd_fcmgt_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmgt h0, h1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmgt s0, s1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmgt d0, d1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmgt v0.4h, v1.4h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmgt v0.2s, v1.2s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmgt v0.8h, v1.8h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmgt v0.4s, v1.4s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmgt_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmgt v0.2d, v1.2d, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmeq_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmeq h0, h1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmeq s0, s1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmeq d0, d1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmeq v0.4h, v1.4h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmeq v0.2s, v1.2s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmeq v0.8h, v1.8h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmeq v0.4s, v1.4s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmeq_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmeq v0.2d, v1.2d, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmlt_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmlt h0, h1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmlt s0, s1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmlt d0, d1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmlt v0.4h, v1.4h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmlt v0.2s, v1.2s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmlt v0.8h, v1.8h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmlt v0.4s, v1.4s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmlt_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmlt v0.2d, v1.2d, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmge_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmge h0, h1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmge s0, s1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmge d0, d1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmge v0.4h, v1.4h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmge v0.2s, v1.2s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmge v0.8h, v1.8h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmge v0.4s, v1.4s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmge_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmge v0.2d, v1.2d, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

ARM64_HW_TEMPLATE void simd_fcmle_h_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmle h0, h1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_s_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmle s0, s1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_d_scalar_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfcmle d0, d1, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_4h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmle v0.4h, v1.4h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_2s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmle v0.2s, v1.2s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_8h_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmle v0.8h, v1.8h, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_4s_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmle v0.4s, v1.4s, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fcmle_2d_zero(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfcmle v0.2d, v1.2d, #0.0\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD FP reduce ---------- */

ARM64_HW_TEMPLATE void simd_fmaxnmv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxnmv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxnmv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxnmv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxnmv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fmaxv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmaxv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminnmv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminnmv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminnmv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminnmv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminv_h_4h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminv h0, v1.4h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminv_h_8h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminv h0, v1.8h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_fminv_s_4s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfminv s0, v1.4s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_h_2h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfaddp h0, v1.2h\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_s_2s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfaddp s0, v1.2s\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void simd_faddp_d_2d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfaddp d0, v1.2d\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- SIMD logical ---------- */
ARM64_HW_TEMPLATE void simd_and_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nand v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_and_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nand v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bic_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bic_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_orr_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\norr v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_orr_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\norr v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_orn_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\norn v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_orn_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\norn v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_eor_8b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\neor v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_eor_16b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\neor v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bsl_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbsl v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bsl_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbsl v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bit_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbit v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bit_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbit v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bif_8b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbif v0.8b, v1.8b, v2.8b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void simd_bif_16b_accumulate(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nldr q2, [%[input2]]\nbif v0.16b, v1.16b, v2.16b\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- Scalar FP binary ---------- */

ARM64_HW_TEMPLATE void fp_fmul_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmul_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmul_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmul d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fdiv_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fdiv_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fdiv_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfdiv d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfadd d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfsub d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmax_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmax_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmax_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmax d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmin_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmin_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmin_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmin d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmaxnm_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmaxnm_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fmaxnm_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfmaxnm d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fminnm_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fminnm_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fminnm_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfminnm d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmul_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfnmul h0, h1, h2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmul_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfnmul s0, s1, s2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmul_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nfnmul d0, d1, d2\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input5] "r"(output)
			 : "v0", "v1", "v2", "memory");
}

/* ---------- Scalar FP unary ---------- */

ARM64_HW_TEMPLATE void fp_fmov_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmov h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fmov_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmov s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fmov_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nfmov d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fabs_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfabs h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fabs_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfabs s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fabs_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfabs d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fneg_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfneg h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fneg_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfneg s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fneg_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfneg d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fsqrt_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfsqrt h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fsqrt_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfsqrt s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_fsqrt_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfsqrt d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintn_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintn h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintn_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintn s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintn_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintn d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintp_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintp h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintp_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintp s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintp_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintp d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintm_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintm h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintm_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintm s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintm_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintm d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintz_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintz h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintz_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintz s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintz_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintz d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinta_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinta h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinta_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinta s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinta_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinta d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintx_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintx h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintx_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintx s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frintx_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrintx d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinti_h_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinti h0, h1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinti_s_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinti s0, s1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}
ARM64_HW_TEMPLATE void fp_frinti_d_merge(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q0, [%[input0]]\nldr q1, [%[input1]]\nfrinti d0, d1\nstr q0, [%[input5]]"
			 :
			 : [input0] "r"(input0), [input1] "r"(input1), [input5] "r"(output)
			 : "v0", "v1", "memory");
}

/* ---------- Scalar FP ternary ---------- */

ARM64_HW_TEMPLATE void fp_fmadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmadd h0, h1, h2, h3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fmadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmadd s0, s1, s2, s3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fmadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmadd d0, d1, d2, d3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fmsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmsub h0, h1, h2, h3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fmsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmsub s0, s1, s2, s3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fmsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfmsub d0, d1, d2, d3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmadd_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmadd h0, h1, h2, h3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmadd_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmadd s0, s1, s2, s3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmadd_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmadd d0, d1, d2, d3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmsub_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmsub h0, h1, h2, h3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmsub_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmsub s0, s1, s2, s3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}
ARM64_HW_TEMPLATE void fp_fnmsub_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input1]]\nldr q2, [%[input2]]\nldr q3, [%[input3]]\nfnmsub d0, d1, d2, d3\nstr q0, [%[input5]]"
			 :
			 : [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input5] "r"(output)
			 : "v0", "v1", "v2", "v3", "memory");
}

/* ---------- Scalar FP compare ---------- */

ARM64_HW_TEMPLATE void fp_fcmp_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmp h1, h2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmp_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmp s1, s2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmp_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmp d1, d2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmpe h1, h2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmpe s1, s2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nldr q2, [%[input1]]\nfcmpe d1, d2\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "v1", "v2", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmp_zero_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmp h1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmp_zero_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmp s1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmp_zero_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmp d1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_zero_h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmpe h1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_zero_s(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmpe s1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}
ARM64_HW_TEMPLATE void fp_fcmpe_zero_d(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ldr q1, [%[input0]]\nfcmpe d1, #0.0\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v1", "cc", "memory");
}

/* ======================== 数据处理立即数指令模板 ======================== */

/* ---------- move wide ---------- */

ARM64_HW_TEMPLATE void movn_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %w[result0], %w[input1], %w[input2]\nmvn %w[result0], %w[result0]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void movn_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[result0], %[input1], %[input2]\nmvn %[result0], %[result0]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void movz_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %w[result0], %w[input1], %w[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void movz_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[result0], %[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void movk_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %w[work1], %w[input1], %w[input2]\nmov %w[work3], #0xffff\nlslv %w[work3], %w[work3], %w[input2]\nbic %w[result0], %w[input0], %w[work3]\norr %w[result0], %w[result0], %w[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void movk_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[work1], %[input1], %[input2]\nmov %[work3], #0xffff\nlslv %[work3], %[work3], %[input2]\nbic %[result0], %[input0], %[work3]\norr %[result0], %[result0], %[work1]"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}

/* ---------- add/sub with carry ---------- */

ARM64_HW_TEMPLATE void adc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nadc %w[result0], %w[input0], %w[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void adc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nadc %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void adcs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nadcs %w[result1], %w[input0], %w[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void adcs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nadcs %[result1], %[input0], %[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void sbc_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nsbc %w[result0], %w[input0], %w[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void sbc_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nsbc %[result0], %[input0], %[input1]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void sbcs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nsbcs %w[result1], %w[input0], %w[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void sbcs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msr nzcv, %[input2]\nsbcs %[result1], %[input0], %[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}

/* ---------- bit operations and population count ---------- */

ARM64_HW_TEMPLATE void rbit_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rbit %w[result0], %w[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rbit_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rbit %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rev16_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rev16 %w[result0], %w[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rev16_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rev16 %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rev32_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rev %w[result0], %w[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rev32_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rev32 %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void rev64_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rev %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void clz_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clz %w[result0], %w[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void clz_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("clz %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void cls_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cls %w[result0], %w[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void cls_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cls %[result0], %[input0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0));
}
ARM64_HW_TEMPLATE void cnt_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("movi v0.2d, #0\nfmov s0, %w[input0]\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov %w[result0], v0.b[0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v0");
}
ARM64_HW_TEMPLATE void cnt_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("movi v0.2d, #0\nfmov d0, %[input0]\ncnt v0.8b, v0.8b\naddv b0, v0.8b\numov %w[result0], v0.b[0]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "v0");
}

/* ---------- multiply-add and absolute value ---------- */

ARM64_HW_TEMPLATE void madd_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("madd %w[result0], %w[input0], %w[input1], %w[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void madd_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("madd %[result0], %[input0], %[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void msub_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msub %w[result0], %w[input0], %w[input1], %w[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void msub_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("msub %[result0], %[input0], %[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void abs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input0], #0\ncneg %w[result0], %w[input0], mi"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "cc");
}
ARM64_HW_TEMPLATE void abs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %[input0], #0\ncneg %[result0], %[input0], mi"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0)
			 : "cc");
}
ARM64_HW_TEMPLATE void smaddl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("smaddl %[result0], %w[input0], %w[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void smsubl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("smsubl %[result0], %w[input0], %w[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void smulh(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("smulh %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void umaddl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("umaddl %[result0], %w[input0], %w[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void umsubl(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("umsubl %[result0], %w[input0], %w[input1], %[input2]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2));
}
ARM64_HW_TEMPLATE void umulh(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("umulh %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}

/* ---------- ADD/SUB：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void add_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %w0, %w1, %w2" : "=r"(*(uint64_t *)output) : "r"(input0), "r"(input1));
}
ARM64_HW_TEMPLATE void add_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("add %0, %1, %2" : "=r"(*(uint64_t *)output) : "r"(input0), "r"(input1));
}
ARM64_HW_TEMPLATE void adds_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("adds %w0, %w2, %w3\nmrs %1, nzcv"
				 : "=r"(*(uint64_t *)output), "=r"(*(uint64_t *)((uint8_t *)output + sizeof(uint64_t)))
				 : "r"(input0), "r"(input1) : "cc");
}
ARM64_HW_TEMPLATE void adds_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("adds %0, %2, %3\nmrs %1, nzcv"
				 : "=r"(*(uint64_t *)output), "=r"(*(uint64_t *)((uint8_t *)output + sizeof(uint64_t)))
				 : "r"(input0), "r"(input1) : "cc");
}
ARM64_HW_TEMPLATE void sub_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sub %w0, %w1, %w2" : "=r"(*(uint64_t *)output) : "r"(input0), "r"(input1));
}
ARM64_HW_TEMPLATE void sub_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sub %0, %1, %2" : "=r"(*(uint64_t *)output) : "r"(input0), "r"(input1));
}
ARM64_HW_TEMPLATE void subs_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("subs %w0, %w2, %w3\nmrs %1, nzcv"
				 : "=r"(*(uint64_t *)output), "=r"(*(uint64_t *)((uint8_t *)output + sizeof(uint64_t)))
				 : "r"(input0), "r"(input1) : "cc");
}
ARM64_HW_TEMPLATE void subs_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("subs %0, %2, %3\nmrs %1, nzcv"
				 : "=r"(*(uint64_t *)output), "=r"(*(uint64_t *)((uint8_t *)output + sizeof(uint64_t)))
				 : "r"(input0), "r"(input1) : "cc");
}

/* ---------- 逻辑运算：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void and_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("and %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void and_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("and %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void bic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bic %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void bic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bic %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void orr_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("orr %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void orr_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("orr %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void orn_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("orn %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void orn_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("orn %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void eor_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("eor %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void eor_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("eor %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void eon_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("eon %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void eon_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("eon %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void ands_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ands %w[result1], %w[input0], %w[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void ands_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("ands %[result1], %[input0], %[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void bics_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bics %w[result1], %w[input0], %w[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void bics_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bics %[result1], %[input0], %[input1]\nmrs %[result0], nzcv"
			 : [result0] "=&r"(*(uint64_t *)((uint8_t *)output + 8)), [result1] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}

/* ---------- 除法、可变移位、旋转和 CRC ---------- */

ARM64_HW_TEMPLATE void udiv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("udiv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void udiv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("udiv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sdiv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sdiv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void sdiv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("sdiv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void lslv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void lslv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lslv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void lsrv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsrv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void lsrv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsrv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void asrv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("asrv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void asrv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("asrv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void rorv_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rorv %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void rorv_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rorv %[result0], %[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32b(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32b %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32h(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32h %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32w(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32w %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32x(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32x %w[result0], %w[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32cb(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32cb %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32ch(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32ch %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32cw(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32cw %w[result0], %w[input0], %w[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}
ARM64_HW_TEMPLATE void crc32cx(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("crc32cx %w[result0], %w[input0], %[input1]"
			 : [result0] "=r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1));
}

/* ---------- MIN/MAX：立即数类与寄存器类共享 ---------- */

ARM64_HW_TEMPLATE void smax_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input0], %w[input1]\ncsel %w[result0], %w[input0], %w[input1], gt"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void smax_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %[input0], %[input1]\ncsel %[result0], %[input0], %[input1], gt"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void umax_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input0], %w[input1]\ncsel %w[result0], %w[input0], %w[input1], hi"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void umax_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %[input0], %[input1]\ncsel %[result0], %[input0], %[input1], hi"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void smin_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input0], %w[input1]\ncsel %w[result0], %w[input0], %w[input1], lt"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void smin_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %[input0], %[input1]\ncsel %[result0], %[input0], %[input1], lt"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void umin_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %w[input0], %w[input1]\ncsel %w[result0], %w[input0], %w[input1], lo"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}
ARM64_HW_TEMPLATE void umin_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("cmp %[input0], %[input1]\ncsel %[result0], %[input0], %[input1], lo"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1)
			 : "cc");
}

/* ---------- EXTR 与位域 ---------- */

ARM64_HW_TEMPLATE void extract_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("neg %w[work3], %w[input2]\nlslv %w[work3], %w[input0], %w[work3]\nlsrv %w[result0], %w[input1], %w[input2]\ncmp %w[input2], #0\ncsel %w[work3], wzr, %w[work3], eq\norr %w[result0], %w[result0], %w[work3]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void extract_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("neg %[work3], %[input2]\nlslv %[work3], %[input0], %[work3]\nlsrv %[result0], %[input1], %[input2]\ncmp %[input2], #0\ncsel %[work3], xzr, %[work3], eq\norr %[result0], %[result0], %[work3]"
			 : [result0] "=&r"(*(uint64_t *)output), [work3] "=&r"(input3)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2)
			 : "cc");
}
ARM64_HW_TEMPLATE void sbfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsrv w6, %w[input0], %w[input4]\nand w6, w6, #1\nneg w6, w6\nbic w6, w6, %w[input2]\nrorv %w[result0], %w[input0], %w[input3]\nand %w[result0], %w[result0], %w[input1]\nand %w[result0], %w[result0], %w[input2]\norr %w[result0], %w[result0], w6"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x6");
}
ARM64_HW_TEMPLATE void sbfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("lsrv x6, %[input0], %[input4]\nand x6, x6, #1\nneg x6, x6\nbic x6, x6, %[input2]\nrorv %[result0], %[input0], %[input3]\nand %[result0], %[result0], %[input1]\nand %[result0], %[result0], %[input2]\norr %[result0], %[result0], x6"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x6");
}
ARM64_HW_TEMPLATE void bfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bic w6, %w[input0], %w[input3]\nbic %w[result0], %w[input0], %w[input2]\nrorv %w[work1], %w[input1], %w[input4]\nand %w[work1], %w[work1], %w[input2]\norr %w[result0], %w[result0], %w[work1]\nand %w[result0], %w[result0], %w[input3]\norr %w[result0], %w[result0], w6"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x6");
}
ARM64_HW_TEMPLATE void bfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("bic x6, %[input0], %[input3]\nbic %[result0], %[input0], %[input2]\nrorv %[work1], %[input1], %[input4]\nand %[work1], %[work1], %[input2]\norr %[result0], %[result0], %[work1]\nand %[result0], %[result0], %[input3]\norr %[result0], %[result0], x6"
			 : [result0] "=&r"(*(uint64_t *)output), [work1] "=&r"(input1)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3), [input4] "r"(input4)
			 : "x6");
}
ARM64_HW_TEMPLATE void ubfm_dynamic_w32(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rorv %w[result0], %w[input0], %w[input3]\nand %w[result0], %w[result0], %w[input1]\nand %w[result0], %w[result0], %w[input2]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3));
}
ARM64_HW_TEMPLATE void ubfm_dynamic_w64(uint64_t input0, uint64_t input1, uint64_t input2, uint64_t input3, uint64_t input4, void *output)
{
	asm volatile("rorv %[result0], %[input0], %[input3]\nand %[result0], %[result0], %[input1]\nand %[result0], %[result0], %[input2]"
			 : [result0] "=&r"(*(uint64_t *)output)
			 : [input0] "r"(input0), [input1] "r"(input1), [input2] "r"(input2), [input3] "r"(input3));
}
/* ---------- 固定立即数位域模板 ---------- */
// clang-format on

#undef ARM64_HW_TEMPLATE

#endif