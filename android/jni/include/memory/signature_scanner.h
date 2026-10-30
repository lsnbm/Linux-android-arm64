#pragma once
#include "../base/memory_utils.h"
#include <fstream>
#include <sstream>

namespace SignatureScanner
{
    inline constexpr int SIG_MAX_RANGE = 1200;
    inline constexpr size_t SIG_BUFFER_SIZE = 0x8000;
    inline constexpr const char *SIG_DEFAULT_FILE = "Signature.txt";

    struct SigElement
    {
        std::vector<uint8_t> bytes;
        std::vector<bool> mask;
        bool empty() const
        {
            return bytes.empty();
        }
        size_t size() const
        {
            return bytes.size();
        }
        void clear()
        {
            bytes.clear();
            mask.clear();
        }
    };

    struct SigFilterResult
    {
        bool success = false;
        int changedCount = 0;
        int totalCount = 0;
        std::string oldSignature;
        std::string newSignature;
    };

    namespace
    {
        std::string NormalizeSigFileName(const char *filename)
        {
            if (filename != nullptr && *filename != '\0') return std::string(filename);
            return std::string(SIG_DEFAULT_FILE);
        }

        bool IsAbsoluteSigPath(std::string_view path)
        {
            return !path.empty() && path.front() == '/';
        }

        std::string ResolveSigPath(std::string_view path)
        {
            if (IsAbsoluteSigPath(path)) return std::string(path);
            return std::string("/data/akernel/") + std::string(path);
        }

        std::string FormatSignature(const SigElement &sig)
        {
            if (sig.empty()) return "";
            std::string result;
            result.reserve(sig.size() * 4);
            for (size_t i = 0; i < sig.bytes.size(); ++i)
            {
                if (i > 0) result += ' ';
                if (!sig.mask[i]) result += "??";
                else std::format_to(std::back_inserter(result), "{:02X}h", sig.bytes[i]);
            }
            return result;
        }

        SigElement ParseSignature(std::string_view text)
        {
            SigElement sig;
            if (text.empty()) return sig;

            std::istringstream iss{std::string{text}};
            std::string token;

            while (iss >> token)
            {
                if (token == "??" || token == "?")
                {
                    sig.bytes.push_back(0);
                    sig.mask.push_back(false);
                }
                else
                {
                    std::string hex = token;
                    if (!hex.empty() && std::tolower(static_cast<unsigned char>(hex.back())) == 'h') hex.pop_back();

                    unsigned val = 0;
                    auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), val, 16);
                    if (ec == std::errc() && val <= 0xFF)
                    {
                        sig.bytes.push_back(static_cast<uint8_t>(val));
                        sig.mask.push_back(true);
                    }
                    else
                    {
                        LS_LOGE_TAG_FMT("Signature", "无法解析: '{}'", token);
                        sig.clear();
                        return sig;
                    }
                }
            }
            return sig;
        }

        std::vector<uintptr_t> ScanCore(const SigElement &sig, int rangeOffset)
        {
            std::vector<uintptr_t> matches;
            if (sig.empty()) return matches;

            auto regions = dr->GetScanRegions();
            if (regions.empty()) return matches;

            const size_t sigSize = sig.size();
            std::vector<uint8_t> buffer(SIG_BUFFER_SIZE);
            const size_t step = SIG_BUFFER_SIZE - sigSize + 1;

            for (const auto &[rStart, rEnd] : regions)
            {
                if (rEnd - rStart < sigSize) continue;

                for (uintptr_t addr = rStart; addr <= rEnd - sigSize; addr += step)
                {
                    size_t readSize = std::min(static_cast<size_t>(rEnd - addr), SIG_BUFFER_SIZE);
                    if (readSize < sigSize) break;
                    if (dr->Read(addr, buffer.data(), readSize) != static_cast<int>(readSize)) continue;

                    size_t searchEnd = readSize - sigSize;
                    for (size_t off = 0; off <= searchEnd; ++off)
                    {
                        bool matched = true;
                        for (size_t i = 0; i < sigSize; ++i)
                        {
                            if (sig.mask[i] && buffer[off + i] != sig.bytes[i])
                            {
                                matched = false;
                                break;
                            }
                        }
                        if (matched) matches.push_back(addr + off + rangeOffset);
                    }
                }
            }
            return matches;
        }

        bool ReadSigFile(const char *filename, int &range, std::string &sigText)
        {
            std::ifstream fp(filename);
            if (!fp) return false;

            range = 0;
            sigText.clear();
            std::string line;

            while (std::getline(fp, line))
            {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

                if (line.starts_with("范围:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    auto it = std::ranges::find_if(sub, [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
                    if (it != sub.end()) std::from_chars(&*it, sub.data() + sub.size(), range);
                }
                else if (line.starts_with("特征码:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    if (auto f = sub.find_first_not_of(' '); f != std::string::npos) sigText = sub.substr(f);
                }
            }
            return (range > 0 && !sigText.empty());
        }

        bool WriteSigFile(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            std::ofstream fp(filename);
            if (!fp) return false;
            std::println(fp, "目标地址: 0x{:X}", addr);
            std::println(fp, "范围: {}", range);
            std::println(fp, "总字节: {}", sig.size());
            std::println(fp, "特征码: {}", FormatSignature(sig));
            return !fp.fail();
        }

        bool ReadSigFileWithFallback(const char *filename, int &range, std::string &sigText)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (ReadSigFile(rawName.c_str(), range, sigText)) return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return ReadSigFile(fallback.c_str(), range, sigText);
            }
            return false;
        }

        bool WriteSigFileWithFallback(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (WriteSigFile(rawName.c_str(), addr, range, sig)) return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return WriteSigFile(fallback.c_str(), addr, range, sig);
            }
            return false;
        }
    } // namespace

    inline bool ScanAddressSignature(uintptr_t addr, int range, const char *filename = SIG_DEFAULT_FILE)
    {
        if (range <= 0 || range > SIG_MAX_RANGE)
        {
            LS_LOGE_TAG_FMT("Signature", "range 无效: {} (1-{})", range, SIG_MAX_RANGE);
            return false;
        }
        if (addr < static_cast<uintptr_t>(range))
        {
            LS_LOGE_TAG("Signature", "地址过小会下溢");
            return false;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        SigElement sig;
        sig.bytes.resize(totalSize);

        if (dr->Read(addr - range, sig.bytes.data(), totalSize) != static_cast<int>(totalSize))
        {
            LS_LOGE_TAG_FMT("Signature", "读取失败: 0x{:X}", addr - range);
            return false;
        }

        sig.mask.assign(totalSize, true);

        if (!WriteSigFileWithFallback(filename, addr, range, sig))
        {
            LS_LOGE_TAG_FMT("Signature", "写文件失败: {}", filename);
            return false;
        }

        LS_LOGI_TAG_FMT("Signature", "生成完成: 地址=0x{:X} 范围=±{} 字节={}", addr, range, totalSize);
        return true;
    }

    inline SigFilterResult FilterSignature(uintptr_t addr, const char *filename = SIG_DEFAULT_FILE)
    {
        SigFilterResult result;

        int range = 0;
        std::string oldSigText;
        if (!ReadSigFileWithFallback(filename, range, oldSigText))
        {
            LS_LOGE_TAG_FMT("Signature", "读取文件失败: {}", filename);
            return result;
        }

        SigElement oldSig = ParseSignature(oldSigText);
        if (oldSig.empty())
        {
            LS_LOGE_TAG("Signature", "特征码解析失败");
            return result;
        }

        if (addr < static_cast<uintptr_t>(range))
        {
            LS_LOGE_TAG("Signature", "过滤地址过小");
            return result;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        std::vector<uint8_t> curData(totalSize);

        if (dr->Read(addr - range, curData.data(), totalSize) != static_cast<int>(totalSize))
        {
            LS_LOGE_TAG_FMT("Signature", "过滤读取失败: 0x{:X}", addr - range);
            return result;
        }

        size_t cmpSize = std::min(oldSig.size(), curData.size());
        SigElement newSig;
        newSig.bytes.resize(cmpSize);
        newSig.mask.resize(cmpSize);
        result.totalCount = static_cast<int>(cmpSize);

        for (size_t i = 0; i < cmpSize; ++i)
        {
            if (!oldSig.mask[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
            }
            else if (oldSig.bytes[i] != curData[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
                ++result.changedCount;
            }
            else
            {
                newSig.bytes[i] = curData[i];
                newSig.mask[i] = true;
            }
        }

        result.oldSignature = oldSigText;
        result.newSignature = FormatSignature(newSig);

        if (!WriteSigFileWithFallback(filename, addr, range, newSig))
        {
            LS_LOGE_TAG_FMT("Signature", "过滤结果写文件失败: {}", filename);
            return result;
        }

        result.success = true;
        LS_LOGI_TAG_FMT("Signature", "过滤完成: 总字节={} 变化={}", result.totalCount, result.changedCount);
        return result;
    }

    inline std::vector<uintptr_t> ScanSignature(const char *pattern, int range = 0)
    {
        SigElement sig = ParseSignature(pattern);
        if (sig.empty())
        {
            LS_LOGE_TAG("Signature", "扫描特征码解析失败");
            return {};
        }

        LS_LOGI_TAG_FMT("Signature", "开始扫描: 长度={} 偏移={}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        LS_LOGI_TAG_FMT("Signature", "扫描完成: 找到 {} 个匹配", matches.size());
        return matches;
    }

    inline std::vector<uintptr_t> ScanSignatureFromFile(const char *filename = SIG_DEFAULT_FILE)
    {
        int range = 0;
        std::string sigText;
        if (!ReadSigFileWithFallback(filename, range, sigText))
        {
            LS_LOGE_TAG_FMT("Signature", "扫描读取文件失败: {}", filename);
            return {};
        }

        SigElement sig = ParseSignature(sigText);
        if (sig.empty()) return {};

        LS_LOGI_TAG_FMT("Signature", "开始文件扫描: 长度={} 范围偏移={}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        LS_LOGI_TAG_FMT("Signature", "文件扫描完成: 找到 {} 个匹配", matches.size());

        const std::string outPath = ResolveSigPath(NormalizeSigFileName(filename));
        std::ofstream out(outPath, std::ios::app);
        if (out)
        {
            std::println(out, "\n扫描结果: {} 个", matches.size());
            for (auto a : matches) std::println(out, "0x{:X}", a);
        }

        return matches;
    }
} // namespace SignatureScanner
