// AutoThemePlugin.h - 自动深浅色插件
#pragma once
#include "pch.h"
#include "ThemeConfig.h"

class CAutoThemePlugin;

// 任务栏显示项：当前主题，左键单击切换（可在设置中关闭）
class CThemeItem : public IPluginItem {
public:
    explicit CThemeItem(CAutoThemePlugin* plugin) : m_plugin(plugin) {}

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

private:
    CAutoThemePlugin* m_plugin;
    long long m_lastDbClick = 0;
};

class CAutoThemePlugin : public ITMPlugin {
public:
    CAutoThemePlugin();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    void OnInitialize(ITrafficMonitor* app) override;
    const wchar_t* GetTooltipInfo() override;
    int GetCommandCount() override { return 1; }
    const wchar_t* GetCommandName(int index) override {
        return index == 0 ? L"切换深色/浅色" : nullptr;
    }
    void OnPluginCommand(int index, void* hWnd, void* para) override {
        if (index == 0) ToggleManual();
    }

    void Shutdown();
    void ToggleManual();
    void OnSettingsSaved();
    bool IsCurrentlyLight() const;
    bool ThemeMatches(bool light) const;

private:
    void StartOnce(const std::wstring& dir);
    void StartWorker();
    void WorkerLoop();
    void Tick(const ThemeConfig& cfg);
    void ApplyTheme(bool light);
    void Wake();
    void SetTooltip(const std::wstring& text);

    CThemeItem m_item{this};
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_started{false};
    HANDLE m_wakeEvent = nullptr;

    mutable std::mutex m_mutex;   // tooltip / 手动切换覆盖
    std::wstring m_tooltip;
    wchar_t m_tooltipBuf[256] = {};   // GetTooltipInfo 返回的稳定缓冲
    bool m_override = false;
    long long m_overrideUntil = 0;
    std::atomic<long long> m_nextLocateTry{0};

    std::mutex m_applyMutex;
};