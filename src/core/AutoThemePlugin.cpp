// AutoThemePlugin.cpp - 自动深浅色插件
#include "pch.h"
#include "AutoThemePlugin.h"
#include "Solar.h"
#include "../net/Location.h"
#include "../ui/OptionsDlg.h"

namespace {

HMODULE g_module = nullptr;
CAutoThemePlugin* g_pluginInstance = nullptr;

constexpr wchar_t kPersonalizeKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

int LocalMinute(long long now) {
    time_t t = (time_t)now;
    struct tm lt{};
    localtime_s(&lt, &t);
    return lt.tm_hour * 60 + lt.tm_min;
}

std::wstring ClockText(long long epoch) {
    time_t t = (time_t)epoch;
    struct tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[16] = {};
    swprintf_s(buf, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

bool InDarkWindow(int startMin, int endMin, int minute) {
    if (startMin < endMin) return minute >= startMin && minute < endMin;
    return minute >= startMin || minute < endMin;
}

bool NeedLocation(const ThemeConfig& cfg, long long now) {
    if (!cfg.autoLocate) return false;
    if (cfg.locateTime == 0) return true;
    if (cfg.lat == 0 && cfg.lon == 0) return true;
    return now - cfg.locateTime > 30LL * 24 * 3600;
}

std::wstring ClickHint() {
    return ThemeConfigStore::Instance().Get().clickToggle
        ? L"｜单击切换主题"
        : L"｜右键菜单可切换主题";
}

// 下一次自动切换的时刻（手动切换后到此时刻前不再自动覆盖）
long long NextBoundary(const ThemeConfig& cfg, long long now) {
    if (cfg.mode == 0) {
        time_t t = (time_t)now;
        struct tm lt{};
        localtime_s(&lt, &t);
        lt.tm_hour = 0;
        lt.tm_min = 0;
        lt.tm_sec = 0;
        lt.tm_isdst = -1;
        time_t midnight = mktime(&lt);
        if (midnight == (time_t)-1) return now + 3600;

        for (int day = 0; day < 2; ++day) {
            long long base = (long long)midnight + (long long)day * 86400;
            long long candidates[2] = {
                base + (long long)cfg.darkStartMin * 60,
                base + (long long)cfg.darkEndMin * 60
            };
            long long best = 0;
            for (long long c : candidates) {
                if (c > now && (best == 0 || c < best)) best = c;
            }
            if (best != 0) return best;
        }
        return now + 3600;
    }

    if (cfg.mode == 1 && !(cfg.lat == 0 && cfg.lon == 0)) {
        time_t t = (time_t)now;
        struct tm lt{};
        localtime_s(&lt, &t);
        for (int day = 0; day < 2; ++day) {
            struct tm dayTm = lt;
            dayTm.tm_mday += day;
            dayTm.tm_isdst = -1;
            time_t dayT = mktime(&dayTm);
            if (dayT == (time_t)-1) continue;
            struct tm dayInfo{};
            localtime_s(&dayInfo, &dayT);
            SunTimes st = ComputeSunTimes(dayInfo.tm_year + 1900, dayInfo.tm_mon + 1,
                                          dayInfo.tm_mday, cfg.lat, cfg.lon);
            if (!st.valid || st.polarDay || st.polarNight) continue;
            long long best = 0;
            for (long long c : { st.sunrise, st.sunset }) {
                if (c > now && (best == 0 || c < best)) best = c;
            }
            if (best != 0) return best;
        }
    }

    return now + 3600;
}

} // namespace

extern "C" __declspec(dllexport)
ITMPlugin* TMPluginGetInstance() {
    if (!g_pluginInstance) g_pluginInstance = new CAutoThemePlugin();
    return g_pluginInstance;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        g_module = hModule;
        break;
    case DLL_PROCESS_DETACH:
        // 不在 loader lock 里 join 线程
        if (g_pluginInstance) {
            g_pluginInstance->Shutdown();
            g_pluginInstance = nullptr;
        }
        break;
    }
    return TRUE;
}

// ═══════════════════════════════════════════════
// CThemeItem
// ═══════════════════════════════════════════════
const wchar_t* CThemeItem::GetItemName() const {
    return L"主题模式";
}

const wchar_t* CThemeItem::GetItemId() const {
    return L"theme_mode";
}

const wchar_t* CThemeItem::GetItemLableText() const {
    return L"主题: ";
}

const wchar_t* CThemeItem::GetItemValueText() const {
    return m_plugin->IsCurrentlyLight() ? L"浅色" : L"深色";
}

const wchar_t* CThemeItem::GetItemValueSampleText() const {
    return L"浅色";
}

int CThemeItem::OnMouseEvent(MouseEventType type, int, int, void*, int) {
    if (type == MT_LCLICKED) {
        if (ThemeConfigStore::Instance().Get().clickToggle) m_plugin->ToggleManual();
    } else if (type == MT_DBCLICKED) {
        return 1;   // 拦截双击，避免触发 TrafficMonitor 的默认动作
    }
    return 0;
}

// ═══════════════════════════════════════════════
// CAutoThemePlugin
// ═══════════════════════════════════════════════
CAutoThemePlugin::CAutoThemePlugin() = default;

IPluginItem* CAutoThemePlugin::GetItem(int index) {
    return index == 0 ? &m_item : nullptr;
}

void CAutoThemePlugin::DataRequired() {
    if (!m_started.load()) StartOnce({});
}

void CAutoThemePlugin::OnInitialize(ITrafficMonitor* app) {
    if (!app) return;
    const wchar_t* dir = app->GetPluginConfigDir();
    if (dir && dir[0]) StartOnce(dir);
}

void CAutoThemePlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    if (index == EI_CONFIG_DIR && data && data[0]) StartOnce(data);
}

ITMPlugin::OptionReturn CAutoThemePlugin::ShowOptionsDialog(void* hParent) {
    bool changed = COptionsDlg::Show((HWND)hParent, this);
    return changed ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

const wchar_t* CAutoThemePlugin::GetInfo(PluginInfoIndex index) {
    switch (index) {
    case TMI_NAME:        return L"自动深浅色";
    case TMI_DESCRIPTION: return L"按固定时间或日出日落自动切换系统深浅色主题";
    case TMI_AUTHOR:      return L"ziphooo";
    case TMI_COPYRIGHT:   return L"© 2026 ziphooo";
    case TMI_VERSION:     return L"0.1.0";
    case TMI_URL:         return L"https://github.com/hexiosr";
    default:              return L"";
    }
}

const wchar_t* CAutoThemePlugin::GetTooltipInfo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tooltip.c_str();
}

bool CAutoThemePlugin::IsCurrentlyLight() const {
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKey, L"AppsUseLightTheme",
                     RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return true;
    }
    return value != 0;
}

void CAutoThemePlugin::ApplyTheme(bool light) {
    std::lock_guard<std::mutex> lock(m_applyMutex);

    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kPersonalizeKey, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                        &key, nullptr) == ERROR_SUCCESS) {
        DWORD value = light ? 1 : 0;
        RegSetValueExW(key, L"AppsUseLightTheme", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegSetValueExW(key, L"SystemUsesLightTheme", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
                        SMTO_ABORTIFHUNG, 1000, &result);
}

void CAutoThemePlugin::StartOnce(const std::wstring& dir) {
    if (m_started.exchange(true)) return;

    std::wstring configDir = dir;
    if (configDir.empty() && g_module) {
        wchar_t path[MAX_PATH] = {};
        if (GetModuleFileNameW(g_module, path, MAX_PATH)) {
            configDir = path;
            size_t slash = configDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) configDir.resize(slash);
        }
    }
    if (!configDir.empty()) ThemeConfigStore::Instance().SetDir(configDir);
    ThemeConfigStore::Instance().Load();

    m_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    StartWorker();
}

void CAutoThemePlugin::StartWorker() {
    if (!m_thread.joinable())
        m_thread = std::thread(&CAutoThemePlugin::WorkerLoop, this);
}

void CAutoThemePlugin::Wake() {
    if (m_wakeEvent) SetEvent(m_wakeEvent);
}

void CAutoThemePlugin::Shutdown() {
    m_stop.store(true);
    Wake();
}

void CAutoThemePlugin::WorkerLoop() {
    while (!m_stop.load()) {
        Tick(ThemeConfigStore::Instance().Get());
        if (m_stop.load()) break;
        WaitForSingleObject(m_wakeEvent, 10000);
    }
}

void CAutoThemePlugin::Tick(const ThemeConfig& cfgIn) {
    ThemeConfig cfg = cfgIn;
    const long long now = (long long)time(nullptr);

    if (cfg.mode == 2) {
        SetTooltip(L"自动切换已关闭" + ClickHint());
        return;
    }

    bool overridden = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_override && now >= m_overrideUntil) m_override = false;
        overridden = m_override;
    }

    std::wstring detail;
    int desired = -1;   // -1 未知，0 深色，1 浅色

    if (cfg.mode == 0) {
        const int minute = LocalMinute(now);
        desired = InDarkWindow(cfg.darkStartMin, cfg.darkEndMin, minute) ? 0 : 1;
        detail = L"深色 " + ThemeConfigStore::FormatTime(cfg.darkStartMin)
               + L"–" + ThemeConfigStore::FormatTime(cfg.darkEndMin);
    } else {
        if (NeedLocation(cfg, now)) {
            const bool hasCoords = !(cfg.lat == 0 && cfg.lon == 0);
            if (now < m_nextLocateTry.load()) {
                if (!hasCoords) {
                    SetTooltip(L"等待定位…" + ClickHint());
                    return;
                }
            } else {
                m_nextLocateTry.store(now + 1800);
                LocationResult loc = LocateByIp();
                if (loc.ok) {
                    cfg.lat = loc.lat;
                    cfg.lon = loc.lon;
                    cfg.city = loc.city;
                    cfg.locateTime = now;
                    ThemeConfigStore::Instance().Save(cfg);
                } else if (!hasCoords) {
                    SetTooltip(loc.error + ClickHint());
                    return;
                }
            }
        }

        if (cfg.lat == 0 && cfg.lon == 0) {
            SetTooltip(L"未设置位置" + ClickHint());
            return;
        }

        time_t t = (time_t)now;
        struct tm lt{};
        localtime_s(&lt, &t);
        SunTimes sun = ComputeSunTimes(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                                       cfg.lat, cfg.lon);
        if (!sun.valid) {
            SetTooltip(L"位置无效" + ClickHint());
            return;
        }
        if (sun.polarNight) {
            desired = 0;
            detail = L"极夜";
        } else if (sun.polarDay) {
            desired = 1;
            detail = L"极昼";
        } else {
            desired = (now < sun.sunrise || now >= sun.sunset) ? 0 : 1;
            detail = L"日出 " + ClockText(sun.sunrise) + L" · 日落 " + ClockText(sun.sunset);
        }
        if (!cfg.city.empty()) detail += L"（" + cfg.city + L"）";
    }

    if (!overridden && desired >= 0) {
        const bool light = desired == 1;
        if (light != IsCurrentlyLight()) ApplyTheme(light);
    }

    std::wstring tip = detail;
    if (overridden) tip += L" · 已手动切换";
    tip += ClickHint();
    SetTooltip(tip);
}

void CAutoThemePlugin::SetTooltip(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tooltip = text;
}

void CAutoThemePlugin::ToggleManual() {
    const bool light = !IsCurrentlyLight();
    ApplyTheme(light);

    ThemeConfig cfg = ThemeConfigStore::Instance().Get();
    if (cfg.mode != 2) {
        const long long now = (long long)time(nullptr);
        const long long until = NextBoundary(cfg, now);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_override = true;
        m_overrideUntil = until;
    }

    std::wstring tip = light ? L"已切换为浅色" : L"已切换为深色";
    tip += ClickHint();
    SetTooltip(tip);
    Wake();
}

void CAutoThemePlugin::OnSettingsSaved() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_override = false;
        m_overrideUntil = 0;
    }
    m_nextLocateTry.store(0);
    Wake();
}