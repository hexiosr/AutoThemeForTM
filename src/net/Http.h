// Http.h - WinHTTP 同步请求
#pragma once
#include "pch.h"

namespace Http {

struct Response {
    long status = 0;
    std::string body;
    bool transportOk = false;
    std::wstring error;
};

bool Request(const std::wstring& method, const std::string& url,
             const std::string& extraHeaders, const std::string& body,
             Response& res, int timeoutMs = 6000);

} // namespace Http