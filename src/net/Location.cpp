// Location.cpp - 按 IP 粗略定位
#include "pch.h"
#include "Location.h"
#include "Http.h"
#include "Util.h"

namespace {

bool JsonNumber(const std::string& s, const char* key, double& out) {
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t p = s.find(needle);
    if (p == std::string::npos) return false;
    p = s.find(':', p + needle.size());
    if (p == std::string::npos) return false;
    const char* start = s.c_str() + p + 1;
    char* end = nullptr;
    double v = strtod(start, &end);
    if (end == start) return false;
    out = v;
    return true;
}

bool JsonString(const std::string& s, const char* key, std::string& out) {
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t p = s.find(needle);
    if (p == std::string::npos) return false;
    p = s.find(':', p + needle.size());
    if (p == std::string::npos) return false;
    p = s.find('"', p + 1);
    if (p == std::string::npos) return false;

    std::string value;
    for (size_t i = p + 1; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') { out = value; return true; }
        if (c == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == 'n') value += '\n';
            else if (n == 'r') value += '\r';
            else if (n == 't') value += '\t';
            else if (n == 'u') {
                // 城市名里的 \\uXXXX 少见；跳过 4 位十六进制
                i = (i + 4 < s.size()) ? i + 4 : s.size() - 1;
                value += '?';
            } else {
                value += n;
            }
        } else {
            value += c;
        }
    }
    return false;
}

bool HttpOk(const std::string& url, std::string& body) {
    Http::Response res;
    if (!Http::Request(L"GET", url, "", "", res, 6000)) return false;
    if (!res.transportOk || res.status != 200) return false;
    if (res.body.find("\"success\":false") != std::string::npos) return false;
    if (res.body.find("\"error\":") != std::string::npos) return false;
    body = std::move(res.body);
    return true;
}

} // namespace

LocationResult LocateByIp() {
    LocationResult out;
    std::string body;

    // ipwho.is 有 AAAA 记录，WinHTTP 会走 IPv6；其国内 IPv6 库常定位到错误城市，
    // 所以先走只有 A 记录的 IPv4 接口，最后才用 ipwho.is 兜底。
    if (HttpOk("http://ip-api.com/json/?fields=status,lat,lon,city&lang=zh-CN", body)) {
        double lat = 0, lon = 0;
        if (JsonNumber(body, "lat", lat) && JsonNumber(body, "lon", lon) &&
            (lat != 0 || lon != 0)) {
            out.ok = true;
            out.lat = lat;
            out.lon = lon;
            std::string city;
            if (JsonString(body, "city", city)) out.city = Utf8ToWide(city);
            return out;
        }
    }

    if (HttpOk("https://ipinfo.io/json", body)) {
        std::string loc;
        if (JsonString(body, "loc", loc)) {
            size_t comma = loc.find(',');
            if (comma != std::string::npos) {
                double lat = atof(loc.substr(0, comma).c_str());
                double lon = atof(loc.substr(comma + 1).c_str());
                if (lat != 0 || lon != 0) {
                    out.ok = true;
                    out.lat = lat;
                    out.lon = lon;
                    std::string city;
                    if (JsonString(body, "city", city)) out.city = Utf8ToWide(city);
                    return out;
                }
            }
        }
    }

    if (HttpOk("https://ipwho.is/", body)) {
        double lat = 0, lon = 0;
        if (JsonNumber(body, "latitude", lat) && JsonNumber(body, "longitude", lon) &&
            (lat != 0 || lon != 0)) {
            out.ok = true;
            out.lat = lat;
            out.lon = lon;
            std::string city;
            if (JsonString(body, "city", city)) out.city = Utf8ToWide(city);
            return out;
        }
    }

    out.error = L"定位失败，可在设置里手动填写经纬度";
    return out;
}