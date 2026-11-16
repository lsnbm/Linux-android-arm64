#include "utils/dump_utils.h"

#include "collectors/common_collector.h"
#include "collectors/lenovo_collector.h"
#include "collectors/mi_collector.h"
#include "collectors/oplus_collector.h"
#include "collectors/samsung_collector.h"
#include "collectors/vivo_collector.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{

enum class DeviceChoice
{
    kNone,
    kOplus,
    kLenovo,
    kMiui,
    kSamsung,
    kVivo,
    kInvalid,
};

// 清理用户输入的设备名称，去掉空白并统一转成小写。
std::string normalizeChoice(const char *value)
{
    std::string normalized;
    if (value == nullptr)
    {
        return normalized;
    }

    for (const unsigned char ch : std::string(value))
    {
        if (!std::isspace(ch))
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

// 把编号或设备名称解析成内部设备选择枚举。
DeviceChoice parseDeviceChoice(const std::string &choice)
{
    if (choice == "0" || choice == "none" || choice == "common")
    {
        return DeviceChoice::kNone;
    }
    if (choice == "1" || choice == "oplus" || choice == "oppo" || choice == "realme" || choice == "oneplus")
    {
        return DeviceChoice::kOplus;
    }
    if (choice == "2" || choice == "lenovo")
    {
        return DeviceChoice::kLenovo;
    }
    if (choice == "3" || choice == "mi" || choice == "miui" || choice == "xiaomi")
    {
        return DeviceChoice::kMiui;
    }
    if (choice == "4" || choice == "samsung")
    {
        return DeviceChoice::kSamsung;
    }
    if (choice == "5" || choice == "vivo" || choice == "bbk" || choice == "iqoo")
    {
        return DeviceChoice::kVivo;
    }
    return DeviceChoice::kInvalid;
}

// 打印交互式设备选择菜单。
void printDeviceMenu()
{
    panicdump::logInfo("请选择要导出的设备类型，通用 rc 收集始终会执行:\n");
    panicdump::logInfo("  1. oplus / oppo / realme / oneplus\n");
    panicdump::logInfo("  2. lenovo\n");
    panicdump::logInfo("  3. mi / miui / xiaomi\n");
    panicdump::logInfo("  4. samsung\n");
    panicdump::logInfo("  5. vivo / bbk / iqoo\n");
    panicdump::logInfo("  0. common only\n");
    panicdump::logInfo("输入编号或名称: ");
}

// 从命令行参数或标准输入读取用户选择的设备类型。
DeviceChoice readDeviceChoice(int argc, char *argv[])
{
    if (argc > 1)
    {
        return parseDeviceChoice(normalizeChoice(argv[1]));
    }

    printDeviceMenu();
    char input[64]{};
    if (std::fgets(input, sizeof(input), stdin) == nullptr)
    {
        return DeviceChoice::kInvalid;
    }
    return parseDeviceChoice(normalizeChoice(input));
}

} // namespace

// 程序入口：先选择厂商 collector；通用 rc 和根目录列表始终收集。
int main(int argc, char *argv[])
{
    if (!panicdump::ensureDirectory(panicdump::kDestinationRoot, 0777))
    {
        return 1;
    }
    if (!panicdump::openRuntimeLogFile(panicdump::kDestinationRoot))
    {
        return 1;
    }

    DeviceChoice deviceChoice = readDeviceChoice(argc, argv);
    if (deviceChoice == DeviceChoice::kInvalid)
    {
        panicdump::logError("未知设备类型，请输入 oplus、lenovo、miui、samsung、vivo 或 common。\n");
        panicdump::closeRuntimeLogFile();
        return 1;
    }

    int matchedOplusDumpPathCount = 0;
    int copiedOplusDumpPathCount = 0;
    bool collectedOplusDumps = true;

    int matchedLenovoDumpCount = 0;
    int copiedLenovoDumpCount = 0;
    bool collectedLenovoDumps = true;

    bool copiedMiuiCoredump = false;
    bool handledMiuiCoredump = true;

    bool copiedSamsungResetKlog = false;
    bool copiedSamsungLastKmsg = false;
    bool handledSamsungProcNodes = true;

    int matchedVivoPathCount = 0;
    int copiedVivoPathCount = 0;
    bool collectedVivoDumps = true;

    switch (deviceChoice)
    {
    case DeviceChoice::kOplus:
        collectedOplusDumps = panicdump::collectOplusDumps(matchedOplusDumpPathCount, copiedOplusDumpPathCount);
        break;
    case DeviceChoice::kLenovo:
        collectedLenovoDumps = panicdump::collectLenovoDumps(matchedLenovoDumpCount, copiedLenovoDumpCount);
        break;
    case DeviceChoice::kMiui:
        handledMiuiCoredump = panicdump::copyMiuiCoredumpDirectory(copiedMiuiCoredump);
        break;
    case DeviceChoice::kSamsung:
        handledSamsungProcNodes = panicdump::collectSamsungProcNodes(copiedSamsungResetKlog, copiedSamsungLastKmsg);
        break;
    case DeviceChoice::kVivo:
        collectedVivoDumps = panicdump::collectVivoDumps(matchedVivoPathCount, copiedVivoPathCount);
        break;
    case DeviceChoice::kNone:
        break;
    case DeviceChoice::kInvalid:
        return 1;
    }

    int matchedRcCount = 0;
    int copiedRcCount = 0;
    int listedRootFileCount = 0;
    bool collectedCommonFiles = panicdump::collectCommonFiles(matchedRcCount, copiedRcCount, listedRootFileCount);

    panicdump::logInfo("Oplus 匹配到的 dump 路径数量: %d\n", matchedOplusDumpPathCount);
    panicdump::logInfo("Oplus 成功复制的 dump 路径数量: %d\n", copiedOplusDumpPathCount);
    panicdump::logInfo("Lenovo 匹配到的 dump 路径数量: %d\n", matchedLenovoDumpCount);
    panicdump::logInfo("Lenovo 成功复制的 dump 路径数量: %d\n", copiedLenovoDumpCount);
    panicdump::logInfo("匹配到的 rc 文件数量: %d\n", matchedRcCount);
    panicdump::logInfo("成功复制的 rc 文件数量: %d\n", copiedRcCount);
    panicdump::logInfo("MIUI coredump 复制状态: %s\n", copiedMiuiCoredump ? "已复制" : "未复制");
    panicdump::logInfo("三星 reset_klog 复制状态: %s\n", copiedSamsungResetKlog ? "已复制" : "未复制");
    panicdump::logInfo("三星 last_kmsg 复制状态: %s\n", copiedSamsungLastKmsg ? "已复制" : "未复制");
    panicdump::logInfo("Vivo 匹配到的日志路径数量: %d\n", matchedVivoPathCount);
    panicdump::logInfo("Vivo 成功复制的日志路径数量: %d\n", copiedVivoPathCount);
    panicdump::logInfo("根目录项数量: %d\n", listedRootFileCount);

    bool success = collectedOplusDumps && collectedLenovoDumps && collectedCommonFiles && handledMiuiCoredump && handledSamsungProcNodes && collectedVivoDumps;
    panicdump::closeRuntimeLogFile();
    return success ? 0 : 2;
}