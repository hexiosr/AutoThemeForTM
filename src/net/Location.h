// Location.h - 按 IP 粗略定位
#pragma once
#include "pch.h"

struct LocationResult {
    bool ok = false;
    double lat = 0;
    double lon = 0;
    std::wstring city;
    std::wstring error;
};

// 依次尝试 ip-api.com（IPv4）、ipinfo.io、ipwho.is
LocationResult LocateByIp();