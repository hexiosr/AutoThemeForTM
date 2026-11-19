// ThemeConfig.h - 插件配置（INI）
#pragma once
#include "pch.h"

struct ThemeConfig {
    int mode = 0;              // 0 固定时间，1 日出日落，2 关闭自动
    int darkStartMin = 19 * 60;
    int darkEndMin = 7 * 60;
    bool autoLocate = true;
    bool clickToggle = true;   // 任务栏单击切换主题
    double lat = 0;
    double lon = 0;
    long long locateTime = 0;
    std::wstring city;
};

class ThemeConfigStore {
public:
    static ThemeConfigStore& Instance();

    void SetDir(const std::wstring& dir);
    std::wstring FilePath() const;

    void Load();
    void Save(const ThemeConfig& cfg);
    void UpdateLocation(double lat, double lon, const std::wstring& city,
                        long long locateTime);
    ThemeConfig Get() const;

    static bool ParseTime(const std::wstring& text, int& minutes);
    static std::wstring FormatTime(int minutes);

private:
    ThemeConfigStore() = default;

    mutable std::mutex m_mutex;
    std::mutex m_saveMutex;
    ThemeConfig m_cfg;
    std::wstring m_dir;
};