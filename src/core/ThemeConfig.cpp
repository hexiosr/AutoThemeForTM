// ThemeConfig.cpp - 插件配置（INI）
#include "pch.h"
#include "ThemeConfig.h"

namespace {

constexpr int kDefaultStart = 19 * 60;
constexpr int kDefaultEnd = 7 * 60;

std::wstring ConfigFile(const std::wstring& dir) {
    if (dir.empty()) return L"AutoTheme.ini";
    return dir + L"\\AutoTheme.ini";
}

bool IsDigit(wchar_t c) {
    return c >= L'0' && c <= L'9';
}

void WriteLocation(const std::wstring& file, double lat, double lon,
                   const std::wstring& city, long long locateTime) {
    wchar_t num[64] = {};
    swprintf_s(num, L"%.6f", lat);
    WritePrivateProfileStringW(L"Location", L"Lat", num, file.c_str());
    swprintf_s(num, L"%.6f", lon);
    WritePrivateProfileStringW(L"Location", L"Lon", num, file.c_str());
    swprintf_s(num, L"%lld", locateTime);
    WritePrivateProfileStringW(L"Location", L"LocateTime", num, file.c_str());
    if (city.empty())
        WritePrivateProfileStringW(L"Location", L"City", nullptr, file.c_str());
    else
        WritePrivateProfileStringW(L"Location", L"City", city.c_str(), file.c_str());
}

} // namespace

ThemeConfigStore& ThemeConfigStore::Instance() {
    static ThemeConfigStore inst;
    return inst;
}

void ThemeConfigStore::SetDir(const std::wstring& dir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dir = dir;
}

std::wstring ThemeConfigStore::FilePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ConfigFile(m_dir);
}

bool ThemeConfigStore::ParseTime(const std::wstring& text, int& minutes) {
    size_t i = 0;
    const size_t n = text.size();
    while (i < n && (text[i] == L' ' || text[i] == L'\t')) ++i;

    int hour = 0;
    bool hasHour = false;
    while (i < n && IsDigit(text[i])) {
        hour = hour * 10 + (text[i] - L'0');
        ++i;
        hasHour = true;
    }
    if (!hasHour || i >= n || text[i] != L':') return false;
    ++i;

    int minute = 0;
    bool hasMinute = false;
    while (i < n && IsDigit(text[i])) {
        minute = minute * 10 + (text[i] - L'0');
        ++i;
        hasMinute = true;
    }
    if (!hasMinute) return false;
    while (i < n && (text[i] == L' ' || text[i] == L'\t')) ++i;
    if (i != n) return false;

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    minutes = hour * 60 + minute;
    return true;
}

std::wstring ThemeConfigStore::FormatTime(int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes > 23 * 60 + 59) minutes = 23 * 60 + 59;
    wchar_t buf[16] = {};
    swprintf_s(buf, L"%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

std::wstring ThemeConfigStore::FormatEpochClock(long long epoch) {
    time_t t = (time_t)epoch;
    struct tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[16] = {};
    swprintf_s(buf, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

void ThemeConfigStore::Load() {
    const std::wstring file = FilePath();
    ThemeConfig cfg;

    cfg.mode = (int)GetPrivateProfileIntW(L"Switch", L"Mode", 0, file.c_str());
    if (cfg.mode < 0 || cfg.mode > 2) cfg.mode = 0;

    wchar_t buf[128] = {};
    GetPrivateProfileStringW(L"Switch", L"DarkStart", L"19:00", buf, 128, file.c_str());
    int value = 0;
    if (ParseTime(buf, value)) cfg.darkStartMin = value;

    GetPrivateProfileStringW(L"Switch", L"DarkEnd", L"07:00", buf, 128, file.c_str());
    if (ParseTime(buf, value)) cfg.darkEndMin = value;

    cfg.autoLocate = GetPrivateProfileIntW(L"Location", L"AutoLocate", 1, file.c_str()) != 0;
    cfg.clickAction = (int)GetPrivateProfileIntW(L"Switch", L"ClickAction", -1, file.c_str());
    if (cfg.clickAction < 0 || cfg.clickAction > 2) {
        // 兼容旧版 ClickToggle：勾选=单击切换，未勾选=不切换
        cfg.clickAction = GetPrivateProfileIntW(L"Switch", L"ClickToggle", 1, file.c_str()) != 0 ? 1 : 0;
    }

    GetPrivateProfileStringW(L"Location", L"Lat", L"0", buf, 128, file.c_str());
    cfg.lat = wcstod(buf, nullptr);
    GetPrivateProfileStringW(L"Location", L"Lon", L"0", buf, 128, file.c_str());
    cfg.lon = wcstod(buf, nullptr);
    if (cfg.lat < -90 || cfg.lat > 90) cfg.lat = 0;
    if (cfg.lon < -180 || cfg.lon > 180) cfg.lon = 0;

    GetPrivateProfileStringW(L"Location", L"LocateTime", L"0", buf, 128, file.c_str());
    cfg.locateTime = wcstoll(buf, nullptr, 10);
    if (cfg.locateTime < 0) cfg.locateTime = 0;

    GetPrivateProfileStringW(L"Location", L"City", L"", buf, 128, file.c_str());
    cfg.city = buf;

    if (cfg.darkStartMin == cfg.darkEndMin) {
        cfg.darkStartMin = kDefaultStart;
        cfg.darkEndMin = kDefaultEnd;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg = cfg;
}

void ThemeConfigStore::Save(const ThemeConfig& cfg) {
    std::lock_guard<std::mutex> saveLock(m_saveMutex);
    const std::wstring file = FilePath();
    const int mode = (cfg.mode < 0 || cfg.mode > 2) ? 0 : cfg.mode;

    WritePrivateProfileStringW(L"Switch", L"Mode",
        std::to_wstring(mode).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Switch", L"DarkStart",
        FormatTime(cfg.darkStartMin).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Switch", L"DarkEnd",
        FormatTime(cfg.darkEndMin).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Switch", L"ClickAction",
        std::to_wstring(cfg.clickAction).c_str(), file.c_str());
    WritePrivateProfileStringW(L"Location", L"AutoLocate",
        cfg.autoLocate ? L"1" : L"0", file.c_str());

    WriteLocation(file, cfg.lat, cfg.lon, cfg.city, cfg.locateTime);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg = cfg;
    m_cfg.mode = mode;
}

void ThemeConfigStore::UpdateLocation(double lat, double lon, const std::wstring& city,
                                      long long locateTime) {
    std::lock_guard<std::mutex> saveLock(m_saveMutex);
    WriteLocation(FilePath(), lat, lon, city, locateTime);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_cfg.lat = lat;
    m_cfg.lon = lon;
    m_cfg.city = city;
    m_cfg.locateTime = locateTime;
}

ThemeConfig ThemeConfigStore::Get() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cfg;
}