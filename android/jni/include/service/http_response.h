#pragma once
#include <string>

namespace Http
{
    struct Response
    {
        int status;
        std::string reason;
        std::string body;
        std::string extraHeaders = {};
    };
}
