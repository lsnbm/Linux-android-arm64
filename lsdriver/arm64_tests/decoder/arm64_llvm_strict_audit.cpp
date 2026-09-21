#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Triple.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCCodeEmitter.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCFixup.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

static std::string hex_word(uint32_t word)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(8) << word;
    return output.str();
}

template <typename Range>
static std::string hex_bytes(const Range &bytes)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (auto byte : bytes)
        output << std::setw(2)
               << static_cast<unsigned>(static_cast<uint8_t>(byte));
    return output.str();
}

static const char *status_name(llvm::MCDisassembler::DecodeStatus status)
{
    switch (status)
    {
    case llvm::MCDisassembler::Success: return "success";
    case llvm::MCDisassembler::SoftFail: return "softfail";
    case llvm::MCDisassembler::Fail: return "fail";
    }
    return "unknown";
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <instruction.txt>\n";
        return EXIT_FAILURE;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
    {
        std::cerr << argv[1] << ": cannot open input\n";
        return EXIT_FAILURE;
    }
    std::string data((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    if (input.bad())
    {
        std::cerr << argv[1] << ": read failure\n";
        return EXIT_FAILURE;
    }
    if (data.empty())
    {
        std::cerr << argv[1] << ": input is empty\n";
        return EXIT_FAILURE;
    }

    std::vector<uint32_t> words;
    size_t offset = 0;
    size_t line_number = 1;
    while (offset < data.size())
    {
        size_t newline = data.find('\n', offset);
        bool terminated = newline != std::string::npos;
        if (!terminated)
            newline = data.size();
        std::string_view line(data.data() + offset, newline - offset);

        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (!line.empty())
        {
            if (line.size() != 8)
            {
                std::cerr << argv[1] << ':' << line_number
                          << ": expected exactly eight hex digits followed by LF or CRLF\n";
                return EXIT_FAILURE;
            }
            uint32_t word = 0;
            for (char character : line)
            {
                int digit = character >= '0' && character <= '9' ? character - '0' :
                            character >= 'a' && character <= 'f' ? character - 'a' + 10 :
                            character >= 'A' && character <= 'F' ? character - 'A' + 10 : -1;
                if (digit < 0)
                {
                    std::cerr << argv[1] << ':' << line_number
                              << ": expected exactly eight hex digits followed by LF or CRLF\n";
                    return EXIT_FAILURE;
                }
                word = (word << 4) | static_cast<uint32_t>(digit);
            }
            words.push_back(word);
        }
        offset = terminated ? newline + 1 : data.size();
        line_number++;
    }

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
        target->createMCSubtargetInfo(triple.str(), "generic", "+lse,+rcpc"));
    if (!registers || !instructions || !subtarget)
    {
        std::cerr << "failed to create AArch64 MC metadata\n";
        return EXIT_FAILURE;
    }

    llvm::MCTargetOptions options;
    std::unique_ptr<llvm::MCAsmInfo> assembly_info(
        target->createMCAsmInfo(*registers, triple.str(), options));
    if (!assembly_info)
    {
        std::cerr << "failed to create AArch64 MCAsmInfo\n";
        return EXIT_FAILURE;
    }

    llvm::MCContext context(
        triple, assembly_info.get(), registers.get(), subtarget.get(),
        nullptr, &options);
    std::unique_ptr<llvm::MCDisassembler> disassembler(
        target->createMCDisassembler(*subtarget, context));
    std::unique_ptr<llvm::MCCodeEmitter> emitter(
        target->createMCCodeEmitter(*instructions, context));
    std::unique_ptr<llvm::MCInstPrinter> printer(target->createMCInstPrinter(
        triple, assembly_info->getAssemblerDialect(), *assembly_info,
        *instructions, *registers));
    if (!disassembler || !emitter || !printer)
    {
        std::cerr << "failed to create AArch64 disassembler/code emitter\n";
        return EXIT_FAILURE;
    }

    std::cout << "index\tinput_raw\tinput_bytes_le\topcode\tdecode_status"
                 "\tdecode_size\tencoded_raw\tencoded_bytes_le\tfixups"
                 "\tidentity\toperand_count\toperands\timmediates\tassembly\n";

    size_t rejected = 0;
    for (size_t index = 0; index < words.size(); index++)
    {
        uint32_t word = words[index];
        std::array<uint8_t, 4> bytes{{
            static_cast<uint8_t>(word),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word >> 16),
            static_cast<uint8_t>(word >> 24)
        }};
        llvm::MCInst instruction;
        uint64_t size = 0;
        auto status = disassembler->getInstruction(
            instruction, size,
            llvm::ArrayRef<uint8_t>(bytes.data(), bytes.size()),
            static_cast<uint64_t>(index) * 4, llvm::nulls());

        std::cout << index << '\t' << hex_word(word) << '\t'
                  << hex_bytes(bytes) << '\t';
        if (status != llvm::MCDisassembler::Success || size != bytes.size())
        {
            std::cout << "-\t" << status_name(status) << '\t' << size
                      << "\t-\t-\t-\t0\t0\t-\t-\t-\n";
            rejected++;
            continue;
        }

        std::string encoded;
        llvm::raw_string_ostream encoded_stream(encoded);
        llvm::SmallVector<llvm::MCFixup, 0> fixups;
        emitter->encodeInstruction(
            instruction, encoded_stream, fixups, *subtarget);
        encoded_stream.flush();

        bool identity = encoded.size() == bytes.size() && fixups.empty() &&
                        !context.hadError();
        if (identity)
        {
            for (size_t byte_index = 0; byte_index < bytes.size(); byte_index++)
                if (static_cast<uint8_t>(encoded[byte_index]) != bytes[byte_index])
                {
                    identity = false;
                    break;
                }
        }

        std::cout << instructions->getName(instruction.getOpcode()).str()
                  << '\t' << status_name(status) << '\t' << size << '\t';
        if (encoded.size() == bytes.size())
        {
            std::cout << hex_word(
                static_cast<uint8_t>(encoded[0]) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[1])) << 8) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[2])) << 16) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[3])) << 24));
        }
        else
        {
            std::cout << '-';
        }
        std::cout << '\t' << (encoded.empty() ? "-" : hex_bytes(encoded))
                  << '\t' << fixups.size() << '\t' << (identity ? 1 : 0)
                  << '\t' << instruction.getNumOperands() << '\t';
        for (unsigned operand_index = 0;
             operand_index < instruction.getNumOperands(); operand_index++)
        {
            if (operand_index)
                std::cout << ';';
            if (instruction.getOperand(operand_index).isReg())
                std::cout << "r:" << registers->getName(
                    instruction.getOperand(operand_index).getReg());
            else if (instruction.getOperand(operand_index).isImm())
                std::cout << "i:" << instruction.getOperand(operand_index).getImm();
            else if (instruction.getOperand(operand_index).isSFPImm())
                std::cout << "sf:" << instruction.getOperand(operand_index).getSFPImm();
            else if (instruction.getOperand(operand_index).isDFPImm())
                std::cout << "df:" << instruction.getOperand(operand_index).getDFPImm();
            else if (instruction.getOperand(operand_index).isExpr())
                std::cout << 'e';
            else if (instruction.getOperand(operand_index).isInst())
                std::cout << 's';
            else
                std::cout << 'x';
        }
        std::cout << '\t';
        bool first_immediate = true;
        for (unsigned operand_index = 0;
             operand_index < instruction.getNumOperands(); operand_index++)
        {
            if (!instruction.getOperand(operand_index).isImm())
                continue;
            if (!first_immediate)
                std::cout << ';';
            std::cout << instruction.getOperand(operand_index).getImm();
            first_immediate = false;
        }
        std::cout << '\t';
        std::string assembly;
        llvm::raw_string_ostream assembly_stream(assembly);
        printer->printInst(
            &instruction, static_cast<uint64_t>(index) * 4, "", *subtarget,
            assembly_stream);
        assembly_stream.flush();
        for (char &character : assembly)
            if (character == '\t' || character == '\n' || character == '\r')
                character = ' ';
        std::cout << assembly
                  << '\n';
    }

    std::cerr << "LLVM AArch64 strict audit: rows=" << words.size()
              << " rejected=" << rejected << '\n';
    return EXIT_SUCCESS;
}
