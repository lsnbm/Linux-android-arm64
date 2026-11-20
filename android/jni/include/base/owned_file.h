#pragma once
#include <cstdio>
#include <memory>

namespace Io
{
    struct CloseFile
    {
        void operator()(std::FILE *file) const noexcept { std::fclose(file); }
    };

    using File = std::unique_ptr<std::FILE, CloseFile>;
}