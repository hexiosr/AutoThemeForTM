// OptionsDlg.cpp - 设置对话框（纯 Win32，无 RC 模板）
#include "pch.h"
#include "OptionsDlg.h"
#include "AutoThemePlugin.h"
#include "Solar.h"
#include "ThemeConfig.h"
#include "../net/Location.h"

namespace {

enum {
    IDC_COMBO_MODE = 1001,
    IDC_COMBO_START_H,
    IDC_COMBO_START_M,
    IDC_COMBO_END_H,
    IDC_COMBO_END_M,
    IDC_CHECK_AUTO,
    IDC_EDIT_LAT,
    IDC_EDIT_LON,
    IDC_LBL_STATUS,
    IDC_BTN_OK,
    IDC_BTN_APPLY,
    IDC_BTN_CANCEL,
    IDC_CHECK_CLICK,
};

constexpr int kMinuteStep = 5;

struct DlgState {
    bool result = false;
    bool saved = false;
    bool closed = false;
    CAutoThemePlugin* plugin = nullptr;

    HFONT hFont = nullptr;
    bool ownFont = false;

    HWND hComboMode = nullptr;
    HWND hComboStartH = nullptr;
    HWND hComboStartM = nullptr;
    HWND hComboEndH = nullptr;
    HWND hComboEndM = nullptr;
    HWND hCheckAuto = nullptr;
    HWND hCheckClick = nullptr;
    HWND hEditLat = nullptr;
    HWND hEditLon = nullptr;
    HWND hLblStatus = nullptr;
    HWND hLblStart = nullptr;
    HWND hLblEnd = nullptr;
    HWND hLblLat = nullptr;
    HWND hLblLon = nullptr;

    bool located = false;
    double locLat = 0;
    double locLon = 0;
    std::wstring locCity;
    long long locTime = 0;
};

int GetComboTime(HWND hHour, HWND hMinute) {
    int hour = (int)SendMessageW(hHour, CB_GETCURSEL, 0, 0);
    int minute = (int)SendMessageW(hMinute, CB_GETCURSEL, 0, 0);
    if (hour < 0) hour = 0;
    if (minute < 0) minute = 0;
    return hour * 60 + minute * kMinuteStep;
}

void SetComboTime(HWND hHour, HWND hMinute, int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes > 23 * 60 + 59) minutes = 23 * 60 + 59;
    int mIndex = (minutes % 60 + kMinuteStep / 2) / kMinuteStep;
    if (mIndex > 60 / kMinuteStep - 1) mIndex = 60 / kMinuteStep - 1;
    SendMessageW(hHour, CB_SETCURSEL, minutes / 60, 0);
    SendMessageW(hMinute, CB_SETCURSEL, mIndex, 0);
}

void AlignVertically(HWND label, HWND ctl) {
    if (!label || !ctl) return;
    HWND parent = GetParent(ctl);
    RECT r{}, lr{};
    if (!GetWindowRect(ctl, &r) || !GetWindowRect(label, &lr)) return;
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&r), 2);
    MapWindowPoints(HWND_DESKTOP, parent, reinterpret_cast<LPPOINT>(&lr), 2);
    SetWindowPos(label, nullptr, lr.left, r.top, lr.right - lr.left, r.bottom - lr.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void AddCtrlImpl(HWND hWnd, HINSTANCE hInst, HFONT font, LPCWSTR cls, LPCWSTR text,
                 DWORD style, int x, int y, int w, int h, int id, HWND* out) {
    HWND hw = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                              x, y, w, h, hWnd, (HMENU)(intptr_t)id, hInst, NULL);
    if (hw && font) SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    if (out) *out = hw;
}

bool g_btnHover[3] = {};
WNDPROC g_btnOldProc = nullptr;

LRESULT CALLBACK OwnerBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    int idx = GetDlgCtrlID(h) - IDC_BTN_OK;
    if (idx >= 0 && idx < 3) {
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_MOUSEMOVE && !g_btnHover[idx]) {
            g_btnHover[idx] = true;
            TRACKMOUSEEVENT t = {};
            t.cbSize = sizeof(t);
            t.dwFlags = TME_LEAVE;
            t.hwndTrack = h;
            TrackMouseEvent(&t);
            InvalidateRect(h, nullptr, TRUE);
        } else if (msg == WM_MOUSELEAVE && g_btnHover[idx]) {
            g_btnHover[idx] = false;
            InvalidateRect(h, nullptr, TRUE);
        }
    }
    return g_btnOldProc ? CallWindowProcW(g_btnOldProc, h, msg, wp, lp)
                        : DefWindowProcW(h, msg, wp, lp);
}

void SetStatus(DlgState* st, const wchar_t* text) {
    if (st && st->hLblStatus) SetWindowTextW(st->hLblStatus, text);
}

void UpdateUiState(DlgState* st);

std::wstring CoordText(double value) {
    wchar_t buf[64] = {};
    swprintf_s(buf, L"%g", value);
    return buf;
}

std::wstring ClockText(long long epoch) {
    time_t t = (time_t)epoch;
    struct tm lt{};
    localtime_s(&lt, &t);
    wchar_t buf[16] = {};
    swprintf_s(buf, L"%02d:%02d", lt.tm_hour, lt.tm_min);
    return buf;
}

// 按坐标计算今日深色时段（日落到日出）
std::wstring DarkWindowText(double lat, double lon) {
    if (lat == 0 && lon == 0) return {};
    time_t t = time(nullptr);
    struct tm lt{};
    localtime_s(&lt, &t);
    SunTimes sun = ComputeSunTimes(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lat, lon);
    if (!sun.valid) return L"无法计算日出日落";
    if (sun.polarNight) return L"深色时段：全天（极夜）";
    if (sun.polarDay) return L"深色时段：无（极昼）";
    return L"深色时段 " + ClockText(sun.sunset) + L" – " + ClockText(sun.sunrise);
}

std::wstring AppendDarkWindow(const std::wstring& text, const std::wstring& dark) {
    return dark.empty() ? text : text + L"\r\n" + dark;
}

bool TryLocate(DlgState* st, HWND hWnd) {
    if (!st) return false;
    SetStatus(st, L"正在定位…");
    UpdateWindow(hWnd);
    HCURSOR oldCursor = SetCursor(LoadCursorW(NULL, IDC_WAIT));
    LocationResult loc = LocateByIp();
    SetCursor(oldCursor);

    if (!loc.ok) {
        SetStatus(st, loc.error.empty() ? L"定位失败，可手动填写经纬度"
                                        : loc.error.c_str());
        return false;
    }

    st->located = true;
    st->locLat = loc.lat;
    st->locLon = loc.lon;
    st->locCity = loc.city;
    st->locTime = (long long)time(nullptr);
    SetWindowTextW(st->hEditLat, CoordText(loc.lat).c_str());
    SetWindowTextW(st->hEditLon, CoordText(loc.lon).c_str());

    UpdateUiState(st);
    return true;
}

void UpdateUiState(DlgState* st) {
    if (!st) return;
    int mode = (int)SendMessageW(st->hComboMode, CB_GETCURSEL, 0, 0);
    if (mode < 0) mode = 0;
    bool fixed = (mode == 0);
    bool solar = (mode == 1);
    bool autoLocate = SendMessageW(st->hCheckAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;

    EnableWindow(st->hComboStartH, fixed);
    EnableWindow(st->hComboStartM, fixed);
    EnableWindow(st->hComboEndH, fixed);
    EnableWindow(st->hComboEndM, fixed);
    EnableWindow(st->hCheckAuto, solar);
    EnableWindow(st->hEditLat, solar && !autoLocate);
    EnableWindow(st->hEditLon, solar && !autoLocate);

    if (fixed) {
        wchar_t buf[160] = {};
        swprintf_s(buf, L"深色时段 %s – %s",
                   ThemeConfigStore::FormatTime(GetComboTime(st->hComboStartH,
                                                             st->hComboStartM)).c_str(),
                   ThemeConfigStore::FormatTime(GetComboTime(st->hComboEndH,
                                                             st->hComboEndM)).c_str());
        SetStatus(st, buf);
    } else if (!solar) {
        bool clickOn = SendMessageW(st->hCheckClick, BM_GETCHECK, 0, 0) == BST_CHECKED;
        SetStatus(st, clickOn ? L"仅手动切换：点击任务栏主题项"
                              : L"仅手动切换：右键菜单");
    } else {
        wchar_t lat[32] = {}, lon[32] = {};
        GetWindowTextW(st->hEditLat, lat, 32);
        GetWindowTextW(st->hEditLon, lon, 32);
        double la = wcstod(lat, nullptr);
        double lo = wcstod(lon, nullptr);
        const std::wstring dark = DarkWindowText(la, lo);
        if (autoLocate) {
            if (la == 0 && lo == 0) {
                SetStatus(st, L"勾选后自动获取位置");
            } else {
                ThemeConfig cfg = ThemeConfigStore::Instance().Get();
                const std::wstring& city = st->located ? st->locCity : cfg.city;
                std::wstring text = L"已定位：";
                if (!city.empty()) text += city + L" · ";
                text += lat;
                text += L", ";
                text += lon;
                SetStatus(st, AppendDarkWindow(text, dark).c_str());
            }
        } else {
            SetStatus(st, dark.empty() ? L"填写所在经纬度，北纬/东经为正" : dark.c_str());
        }
    }
}

void CreateControls(HWND hWnd, DlgState* st) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);

    LOGFONTW lf = {};
    lf.lfHeight = -16;
    lf.lfWeight = FW_NORMAL;
    lstrcpynW(lf.lfFaceName, L"微软雅黑", LF_FACESIZE);
    HFONT hFont = CreateFontIndirectW(&lf);
    st->ownFont = hFont != nullptr;
    if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    st->hFont = hFont;

    auto add = [&](LPCWSTR cls, LPCWSTR text, DWORD style,
                   int x, int y, int w, int h, int id, HWND* out = nullptr) {
        AddCtrlImpl(hWnd, hInst, hFont, cls, text, style, x, y, w, h, id, out);
    };

    add(L"STATIC", L"模式", SS_LEFT | SS_CENTERIMAGE, 48, 38, 86, 30, 0, nullptr);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 38, 150, 200, IDC_COMBO_MODE, &st->hComboMode);

    add(L"STATIC", L"深色开始", SS_LEFT | SS_CENTERIMAGE, 48, 90, 86, 30, 0, &st->hLblStart);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 90, 60, 260, IDC_COMBO_START_H, &st->hComboStartH);
    add(L"STATIC", L":", SS_CENTER | SS_CENTERIMAGE, 206, 90, 14, 30, 0, nullptr);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        220, 90, 60, 260, IDC_COMBO_START_M, &st->hComboStartM);

    add(L"STATIC", L"深色结束", SS_LEFT | SS_CENTERIMAGE, 48, 142, 86, 30, 0, &st->hLblEnd);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        146, 142, 60, 260, IDC_COMBO_END_H, &st->hComboEndH);
    add(L"STATIC", L":", SS_CENTER | SS_CENTERIMAGE, 206, 142, 14, 30, 0, nullptr);
    add(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_BORDER,
        220, 142, 60, 260, IDC_COMBO_END_M, &st->hComboEndM);

    add(L"BUTTON", L"自动定位（按 IP）", BS_AUTOCHECKBOX, 48, 198, 250, 30,
        IDC_CHECK_AUTO, &st->hCheckAuto);

    add(L"STATIC", L"纬度", SS_LEFT | SS_CENTERIMAGE, 48, 246, 40, 30, 0, &st->hLblLat);
    add(L"EDIT", L"", WS_BORDER, 92, 246, 70, 30, IDC_EDIT_LAT, &st->hEditLat);
    add(L"STATIC", L"经度", SS_LEFT | SS_CENTERIMAGE, 176, 246, 40, 30, 0, &st->hLblLon);
    add(L"EDIT", L"", WS_BORDER, 220, 246, 70, 30, IDC_EDIT_LON, &st->hEditLon);

    add(L"BUTTON", L"任务栏单击切换主题", BS_AUTOCHECKBOX, 48, 284, 250, 30,
        IDC_CHECK_CLICK, &st->hCheckClick);

    add(L"STATIC", L"", SS_LEFT, 48, 322, 286, 46, IDC_LBL_STATUS, &st->hLblStatus);

    add(L"BUTTON", L"确定", BS_OWNERDRAW, 28, 372, 92, 38, IDC_BTN_OK);
    add(L"BUTTON", L"应用", BS_OWNERDRAW, 136, 372, 92, 38, IDC_BTN_APPLY);
    add(L"BUTTON", L"取消", BS_OWNERDRAW, 244, 372, 92, 38, IDC_BTN_CANCEL);
    g_btnHover[0] = g_btnHover[1] = g_btnHover[2] = false;
    g_btnOldProc = nullptr;
    for (HWND h = GetWindow(hWnd, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT)) {
        LONG_PTR id = GetWindowLongPtrW(h, GWLP_ID);
        if (id >= IDC_BTN_OK && id <= IDC_BTN_CANCEL) {
            g_btnOldProc = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)OwnerBtnProc);
        }
    }

    ThemeConfig cfg = ThemeConfigStore::Instance().Get();
    SendMessageW(st->hComboMode, CB_ADDSTRING, 0, (LPARAM)L"固定时间");
    SendMessageW(st->hComboMode, CB_ADDSTRING, 0, (LPARAM)L"日出日落");
    SendMessageW(st->hComboMode, CB_ADDSTRING, 0, (LPARAM)L"关闭自动");
    SendMessageW(st->hComboMode, CB_SETCURSEL, (WPARAM)(cfg.mode < 0 || cfg.mode > 2 ? 0 : cfg.mode), 0);

    SendMessageW(st->hEditLat, EM_SETLIMITTEXT, 12, 0);
    SendMessageW(st->hEditLon, EM_SETLIMITTEXT, 12, 0);

    wchar_t timeBuf[8] = {};
    for (int hour = 0; hour < 24; ++hour) {
        swprintf_s(timeBuf, L"%02d", hour);
        SendMessageW(st->hComboStartH, CB_ADDSTRING, 0, (LPARAM)timeBuf);
        SendMessageW(st->hComboEndH, CB_ADDSTRING, 0, (LPARAM)timeBuf);
    }
    for (int minute = 0; minute < 60; minute += kMinuteStep) {
        swprintf_s(timeBuf, L"%02d", minute);
        SendMessageW(st->hComboStartM, CB_ADDSTRING, 0, (LPARAM)timeBuf);
        SendMessageW(st->hComboEndM, CB_ADDSTRING, 0, (LPARAM)timeBuf);
    }
    SetComboTime(st->hComboStartH, st->hComboStartM, cfg.darkStartMin);
    SetComboTime(st->hComboEndH, st->hComboEndM, cfg.darkEndMin);

    SendMessageW(st->hCheckAuto, BM_SETCHECK, cfg.autoLocate ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st->hCheckClick, BM_SETCHECK, cfg.clickToggle ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(st->hEditLat, CoordText(cfg.lat).c_str());
    SetWindowTextW(st->hEditLon, CoordText(cfg.lon).c_str());

    AlignVertically(st->hLblStart, st->hComboStartH);
    AlignVertically(st->hLblEnd, st->hComboEndH);
    AlignVertically(st->hLblLat, st->hEditLat);
    AlignVertically(st->hLblLon, st->hEditLon);

    UpdateUiState(st);
    SetFocus(hWnd);
}

void DrawStyledButton(const DRAWITEMSTRUCT* d) {
    int idx = d->CtlID - IDC_BTN_OK;
    if (idx < 0 || idx > 2) return;
    bool primary = (idx == 0);
    bool pressed = (d->itemState & ODS_SELECTED) != 0;
    bool hover = g_btnHover[idx];

    COLORREF bg;
    if (primary)   bg = pressed ? RGB(0, 84, 189) : hover ? RGB(0, 105, 227) : RGB(0, 122, 255);
    else           bg = pressed ? RGB(209, 209, 214) : hover ? RGB(222, 222, 226) : RGB(233, 233, 235);

    RECT rc = d->rcItem;
    if (rc.right - rc.left < 2) return;

    typedef UINT(WINAPI * PFN_GetDpiForWindow)(HWND);
    static PFN_GetDpiForWindow pGetDpi = (PFN_GetDpiForWindow)(void*)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    UINT dpi = pGetDpi ? pGetDpi(d->hwndItem) : 96;
    int radius = MulDiv(8, dpi, 96);

    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1,
                                  radius, radius);
    HBRUSH br = CreateSolidBrush(bg);
    FillRgn(d->hDC, rgn, br);
    DeleteObject(br);
    DeleteObject(rgn);

    HFONT f = (HFONT)SendMessageW(d->hwndItem, WM_GETFONT, 0, 0);
    HFONT old = f ? (HFONT)SelectObject(d->hDC, f) : nullptr;
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, primary ? RGB(255, 255, 255) : RGB(29, 29, 31));
    wchar_t txt[32] = {};
    GetWindowTextW(d->hwndItem, txt, 32);
    RECT trc = rc;
    DrawTextW(d->hDC, txt, -1, &trc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (old) SelectObject(d->hDC, old);
}

bool ParseCoord(HWND edit, double lo, double hi, double& out) {
    wchar_t buf[64] = {};
    GetWindowTextW(edit, buf, 64);
    wchar_t* end = nullptr;
    double value = wcstod(buf, &end);
    if (end == buf) {
        SetWindowTextW(edit, L"0");
        return false;
    }
    while (*end == L' ' || *end == L'\t') ++end;
    if (*end != 0 || value < lo || value > hi) {
        SetWindowTextW(edit, L"0");
        return false;
    }
    out = value;
    return true;
}

bool SaveFromDialog(DlgState* st, HWND hWnd) {
    ThemeConfig old = ThemeConfigStore::Instance().Get();
    ThemeConfig cfg = old;

    int mode = (int)SendMessageW(st->hComboMode, CB_GETCURSEL, 0, 0);
    if (mode < 0 || mode > 2) mode = 0;
    cfg.mode = mode;

    if (mode == 0) {
        cfg.darkStartMin = GetComboTime(st->hComboStartH, st->hComboStartM);
        cfg.darkEndMin = GetComboTime(st->hComboEndH, st->hComboEndM);
        if (cfg.darkStartMin == cfg.darkEndMin) {
            SetStatus(st, L"开始和结束时间不能相同");
            return false;
        }
    }

    cfg.autoLocate = SendMessageW(st->hCheckAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.clickToggle = SendMessageW(st->hCheckClick, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (mode == 1) {
        if (cfg.autoLocate) {
            if (!st->located && old.lat == 0 && old.lon == 0) {
                if (!TryLocate(st, hWnd)) return false;
            }
            if (st->located) {
                cfg.lat = st->locLat;
                cfg.lon = st->locLon;
                cfg.city = st->locCity;
                cfg.locateTime = st->locTime;
            } else if (!old.autoLocate) {
                cfg.locateTime = 0;   // 沿用手动坐标兜底，后台重新定位
                cfg.city.clear();
            }
        } else {
            double lat = 0, lon = 0;
            if (!ParseCoord(st->hEditLat, -90, 90, lat) ||
                !ParseCoord(st->hEditLon, -180, 180, lon)) {
                SetStatus(st, L"经纬度格式或范围不正确");
                return false;
            }
            if (lat == 0 && lon == 0) {
                SetStatus(st, L"请填写有效的经纬度");
                return false;
            }
            if (lat != old.lat || lon != old.lon || old.autoLocate) {
                cfg.city.clear();
                cfg.locateTime = 0;
            }
            cfg.lat = lat;
            cfg.lon = lon;
        }
    }

    ThemeConfigStore::Instance().Save(cfg);
    if (st->plugin) st->plugin->OnSettingsSaved();

    SetComboTime(st->hComboStartH, st->hComboStartM, cfg.darkStartMin);
    SetComboTime(st->hComboEndH, st->hComboEndM, cfg.darkEndMin);
    SetWindowTextW(st->hEditLat, CoordText(cfg.lat).c_str());
    SetWindowTextW(st->hEditLon, CoordText(cfg.lon).c_str());
    UpdateUiState(st);
    return true;
}

LRESULT CALLBACK DlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        CreateControls(hWnd, st);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(229, 229, 234));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 48, 188, nullptr);
        LineTo(hdc, 298, 188);
        SelectObject(hdc, old);
        DeleteObject(pen);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, RGB(255, 255, 255));
        SetTextColor(hdc, RGB(29, 29, 31));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)lParam;
        if (d->CtlType == ODT_BUTTON) {
            DrawStyledButton(d);
            return TRUE;
        }
        break;
    }
    case DM_GETDEFID:
        return MAKELRESULT(IDC_BTN_OK, DC_HASDEFID);
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && st) {
            st->result = false;
            st->closed = true;
        }
        break;
    case WM_COMMAND: {
        if (!st) break;
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == IDC_COMBO_MODE && code == CBN_SELCHANGE) {
            UpdateUiState(st);
        } else if ((id == IDC_COMBO_START_H || id == IDC_COMBO_START_M ||
                    id == IDC_COMBO_END_H || id == IDC_COMBO_END_M) &&
                   code == CBN_SELCHANGE) {
            UpdateUiState(st);
        } else if (id == IDC_CHECK_AUTO && code == BN_CLICKED) {
            bool checked = SendMessageW(st->hCheckAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
            UpdateUiState(st);
            if (checked && SendMessageW(st->hComboMode, CB_GETCURSEL, 0, 0) == 1)
                TryLocate(st, hWnd);
        } else if ((id == IDC_EDIT_LAT || id == IDC_EDIT_LON) && code == EN_CHANGE) {
            UpdateUiState(st);
        } else if (id == IDC_CHECK_CLICK && code == BN_CLICKED) {
            UpdateUiState(st);
        } else if (id == IDC_BTN_OK) {
            if (SaveFromDialog(st, hWnd)) {
                st->saved = true;
                st->result = true;
                st->closed = true;
            }
        } else if (id == IDC_BTN_CANCEL) {
            st->result = false;
            st->closed = true;
        } else if (id == IDC_BTN_APPLY) {
            if (SaveFromDialog(st, hWnd)) st->saved = true;
        }
        break;
    }
    case WM_CLOSE:
        if (st) { st->result = false; st->closed = true; }
        return 0;
    case WM_DESTROY:
        if (st && st->ownFont && st->hFont) DeleteObject(st->hFont);
        if (st) {
            st->hFont = nullptr;
            st->ownFont = false;
        }
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

ATOM RegisterDlgClass(HINSTANCE hInst) {
    static ATOM atom = 0;
    if (atom == 0) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = L"AutoThemeOptDlg";
        atom = RegisterClassExW(&wc);
    }
    return atom;
}

} // namespace

bool COptionsDlg::Show(HWND hParent, CAutoThemePlugin* plugin) {
    HINSTANCE hInst = hParent
        ? (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE)
        : GetModuleHandleW(NULL);

    RegisterDlgClass(hInst);

    DlgState state;
    state.plugin = plugin;

    RECT rcWnd = { 0, 0, 346, 432 };
    AdjustWindowRectEx(&rcWnd, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_DLGMODALFRAME);
    int W = rcWnd.right - rcWnd.left;
    int H = rcWnd.bottom - rcWnd.top;

    RECT wa{};
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(hParent ? hParent : GetForegroundWindow(),
                                     MONITOR_DEFAULTTONEAREST);
    if (mon && GetMonitorInfoW(mon, &mi))
        wa = mi.rcWork;
    else {
        wa.left = 0;
        wa.top = 0;
        wa.right = GetSystemMetrics(SM_CXSCREEN);
        wa.bottom = GetSystemMetrics(SM_CYSCREEN);
    }

    int workW = wa.right - wa.left;
    int workH = wa.bottom - wa.top;
    int px = wa.left + (workW - W) / 2;
    int py = wa.top + (workH - H) / 2;
    if (hParent) {
        RECT rc{};
        if (GetWindowRect(hParent, &rc)) {
            int cx = rc.left + (rc.right - rc.left - W) / 2;
            int cy = rc.top + (rc.bottom - rc.top - H) / 2;
            if (cx >= wa.left && cy >= wa.top && cx + W <= wa.right && cy + H <= wa.bottom) {
                px = cx;
                py = cy;
            }
        }
    }

    HWND hPrevForeground = GetForegroundWindow();

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"AutoThemeOptDlg",
        L"自动深浅色",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        px, py, W, H,
        hParent, NULL, hInst, &state);
    if (!hDlg) return false;

    if (hParent) EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);

    MSG msg{};
    while (!state.closed && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(hDlg)) break;
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hParent) EnableWindow(hParent, TRUE);
    if (IsWindow(hDlg)) DestroyWindow(hDlg);

    HWND hFocus = nullptr;
    if (hParent && IsWindow(hParent) && IsWindowVisible(hParent))
        hFocus = hParent;
    else if (hPrevForeground && IsWindow(hPrevForeground) && IsWindowVisible(hPrevForeground))
        hFocus = hPrevForeground;
    if (hFocus) SetForegroundWindow(hFocus);

    return state.result || state.saved;
}