#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>

extern "C" {
#include "../../arm64_decode/arm64_decode.h"
}

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

static llvm::MCDisassembler::DecodeStatus decode_with_llvm(
    llvm::MCDisassembler &disassembler, uint32_t raw, llvm::MCInst &instruction)
{
    std::array<uint8_t, 4> bytes{{
        static_cast<uint8_t>(raw),
        static_cast<uint8_t>(raw >> 8),
        static_cast<uint8_t>(raw >> 16),
        static_cast<uint8_t>(raw >> 24),
    }};
    uint64_t size = 0;
    auto status = disassembler.getInstruction(
        instruction, size,
        llvm::ArrayRef<uint8_t>(bytes.data(), bytes.size()), 0, llvm::nulls());
    return size == 4 ? status : llvm::MCDisassembler::Fail;
}

static bool is_known_llvm17_gap(enum arm64_instruction instruction)
{
    return instruction == ARM64_INST_FAMAX_VECTOR ||
           instruction == ARM64_INST_FAMIN_VECTOR ||
           instruction == ARM64_INST_FSCALE_VECTOR;
}

int main()
{
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64Disassembler();

    llvm::Triple triple("aarch64-linux-gnu");
    std::string error;
    const llvm::Target *target =
        llvm::TargetRegistry::lookupTarget("aarch64", triple, error);
    if (!target)
    {
        std::cerr << error << '\n';
        return EXIT_FAILURE;
    }

    std::unique_ptr<llvm::MCRegisterInfo> registers(
        target->createMCRegInfo(triple.str()));
    std::unique_ptr<llvm::MCInstrInfo> instructions(
        target->createMCInstrInfo());
    std::unique_ptr<llvm::MCSubtargetInfo> subtarget(
        target->createMCSubtargetInfo(
            triple.str(), "generic",
            "+v8.6a,+lse,+rcpc,+fullfp16,+fp16fml,+bf16,+i8mm,+crc,+cssc"));
    llvm::MCTargetOptions options;
    std::unique_ptr<llvm::MCAsmInfo> assembly_info(
        target->createMCAsmInfo(*registers, triple.str(), options));
    llvm::MCContext context(
        triple, assembly_info.get(), registers.get(), subtarget.get(),
        nullptr, &options);
    std::unique_ptr<llvm::MCDisassembler> disassembler(
        target->createMCDisassembler(*subtarget, context));
    if (!registers || !instructions || !subtarget || !assembly_info || !disassembler)
    {
        std::cerr << "failed to create LLVM AArch64 decoder\n";
        return EXIT_FAILURE;
    }

    const uint32_t low_fields[] = {
        0,
        (1U << 5) | 2U,
        (31U << 5) | 3U,
        (2U << 5) | 31U,
    };
    const uint32_t groups[] = {5, 7, 8, 9, 0xA, 0xB, 0xD, 0xF};
    std::map<uint32_t, uint64_t> total_by_group;
    std::map<uint32_t, uint64_t> local_only_by_group;
    std::map<uint32_t, uint64_t> llvm_only_by_group;
    std::map<int, uint64_t> known_llvm17_gaps;
    std::map<int, uint64_t> unknown_local_only;
    uint64_t local_accepted = 0;
    uint64_t llvm_accepted = 0;

    for (uint32_t low : low_fields)
    {
        for (uint32_t control = 0; control < (1U << 22); control++)
        {
            uint32_t raw = (control << 10) | low;
            uint32_t group = (raw >> 25) & 0xF;
            bool selected = false;
            for (uint32_t candidate : groups)
                selected |= candidate == group;
            if (!selected)
                continue;

            total_by_group[group]++;
            struct arm64_decoded_instruction decoded{};
            enum arm64_decode_status local_status =
                arm64_decode_instruction(raw, &decoded);
            bool local_ok = local_status == ARM64_DECODE_OK;
            llvm::MCInst instruction;
            auto llvm_status = decode_with_llvm(*disassembler, raw, instruction);
            bool llvm_ok = llvm_status == llvm::MCDisassembler::Success;
            local_accepted += local_ok;
            llvm_accepted += llvm_ok;

            if (local_ok && !llvm_ok)
            {
                local_only_by_group[group]++;
                if (is_known_llvm17_gap(decoded.instruction))
                    known_llvm17_gaps[decoded.instruction]++;
                else
                    unknown_local_only[decoded.instruction]++;
            }
            else if (!local_ok && llvm_ok)
            {
                llvm_only_by_group[group]++;
            }
        }
    }

    uint64_t unknown_total = 0;
    for (const auto &item : unknown_local_only)
        unknown_total += item.second;

    std::cout << "total=";
    uint64_t total = 0;
    for (const auto &item : total_by_group)
        total += item.second;
    std::cout << total << " local_accepted=" << local_accepted
              << " llvm_accepted=" << llvm_accepted
              << " unknown_local_only=" << unknown_total << '\n';
    for (const auto &item : total_by_group)
    {
        std::cout << "group=" << std::hex << item.first << std::dec
                  << " total=" << item.second
                  << " local_only=" << local_only_by_group[item.first]
                  << " llvm_only=" << llvm_only_by_group[item.first] << '\n';
    }
    for (const auto &item : known_llvm17_gaps)
        std::cout << "known_llvm17_gap instruction=" << item.first
                  << " count=" << item.second << '\n';

    if (unknown_total)
    {
        for (const auto &item : unknown_local_only)
            std::cerr << "LOCAL_ONLY instruction=" << item.first
                      << " count=" << item.second << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}