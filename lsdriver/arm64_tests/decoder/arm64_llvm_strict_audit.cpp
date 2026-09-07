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

static int hex_digit(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

static bool parse_word(std::string_view text, uint32_t &word)
{
    if (text.size() == 9 && text.back() == '\r') text.remove_suffix(1);
    if (text.size() != 8) return false;

    word = 0;
    for (char character : text)
    {
        int digit = hex_digit(character);
        if (digit < 0) return false;
        word = (word << 4) | static_cast<uint32_t>(digit);
    }
    return true;
}

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

static std::string operand_dump(
    const llvm::MCInst &instruction,
    const llvm::MCRegisterInfo &registers)
{
    std::ostringstream output;

    for (unsigned index = 0; index < instruction.getNumOperands(); index++)
    {
        if (index) output << ';';

        const llvm::MCOperand &operand = instruction.getOperand(index);
        if (operand.isReg())
        {
            output << "r:" << registers.getName(operand.getReg());
        }
        else if (operand.isImm())
        {
            output << "i:" << operand.getImm();
        }
        else if (operand.isSFPImm())
        {
            output << "sf:" << operand.getSFPImm();
        }
        else if (operand.isDFPImm())
        {
            output << "df:" << operand.getDFPImm();
        }
        else if (operand.isExpr())
        {
            output << "e";
        }
        else if (operand.isInst())
        {
            output << "s";
        }
        else
        {
            output << "x";
        }
    }

    return output.str();
}

static std::string immediate_dump(const llvm::MCInst &instruction)
{
    std::ostringstream output;
    bool first = true;

    for (unsigned index = 0; index < instruction.getNumOperands(); index++)
    {
        const llvm::MCOperand &operand = instruction.getOperand(index);
        if (!operand.isImm()) continue;
        if (!first) output << ';';
        output << operand.getImm();
        first = false;
    }

    return output.str();
}

static bool read_words(const char *path, std::vector<uint32_t> &words)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        std::cerr << path << ": cannot open input\n";
        return false;
    }

    std::string data((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    if (input.bad())
    {
        std::cerr << path << ": read failure\n";
        return false;
    }

    size_t offset = 0;
    size_t line_number = 1;
    while (offset < data.size())
    {
        size_t newline = data.find('\n', offset);
        if (newline == std::string::npos)
        {
            std::cerr << path << ':' << line_number
                      << ": line must end in LF or CRLF\n";
            return false;
        }

        std::string_view line(data.data() + offset, newline - offset);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.remove_suffix(1);
        if (line.empty())
        {
            offset = newline + 1;
            line_number++;
            continue;
        }
        uint32_t word;
        if (!parse_word(line, word))
        {
            std::cerr << path << ':' << line_number
                      << ": expected exactly eight hex digits followed by LF or CRLF\n";
            return false;
        }
        words.push_back(word);
        offset = newline + 1;
        line_number++;
    }

    if (data.empty())
    {
        std::cerr << path << ": input is empty\n";
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <instruction.txt>\n";
        return EXIT_FAILURE;
    }

    std::vector<uint32_t> words;
    if (!read_words(argv[1], words)) return EXIT_FAILURE;

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

    unsigned failures = 0;
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
                      << "\t-\t-\t-\t0\t0\t-\n";
            failures++;
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
                identity = identity &&
                           static_cast<uint8_t>(encoded[byte_index]) ==
                               bytes[byte_index];
        }

        std::cout << instructions->getName(instruction.getOpcode()).str()
                  << '\t' << status_name(status) << '\t' << size << '\t';
        if (encoded.size() == bytes.size())
        {
            uint32_t encoded_word =
                static_cast<uint8_t>(encoded[0]) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[1])) << 8) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[2])) << 16) |
                (static_cast<uint32_t>(static_cast<uint8_t>(encoded[3])) << 24);
            std::cout << hex_word(encoded_word);
        }
        else
        {
            std::cout << '-';
        }
        std::cout << '\t' << (encoded.empty() ? "-" : hex_bytes(encoded))
                  << '\t' << fixups.size() << '\t' << (identity ? 1 : 0)
                  << '\t' << instruction.getNumOperands() << '\t'
                  << operand_dump(instruction, *registers) << '\t'
                  << immediate_dump(instruction) << '\t';
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
        if (!identity) failures++;
    }

    std::cerr << "LLVM AArch64 strict audit: rows=" << words.size()
              << " failures=" << failures << '\n';
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}