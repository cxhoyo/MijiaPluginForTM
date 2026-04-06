// OptionsDlg.cpp - 设置对话框实现（纯Win32，无RC文件）
#include "pch.h"
#include "OptionsDlg.h"
#include "PluginConfig.h"
#include "MiioDevice.h"
#include <commctrl.h>
#include <filesystem>
#include <string>
#include <sstream>

// 控件 ID（不与 Windows SDK 宏冲突）
#define IDC_EDIT_IP           1001
#define IDC_EDIT_TOKEN        1002
#define IDC_EDIT_NAME         1003
#define IDC_EDIT_INTERVAL     1004
#define IDC_CHECK_RECORD      1005
#define IDC_CHECK_LABEL       1006
#define IDC_CHECK_UNIT        1007
#define IDC_COMBO_DECIMAL     1008
#define IDC_STATIC_STATUS     1009
#define IDC_BTN_TEST          1010
#define IDC_STATIC_HISTORY    1011
#define IDC_BTN_CLEARHISTORY  1012
#define IDC_BTN_OK            1013
#define IDC_BTN_CANCEL        1014
#define IDC_SCROLL_CONTAINER  1015

// ─── 辅助：添加控件 ───
static HWND AddCtrl(HWND parent, LPCWSTR cls, LPCWSTR text, DWORD style,
                    int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(intptr_t)id,
                           (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE), NULL);
}

// ─── 对话框状态（每次 Show 调用使用局部结构体传递） ───
struct DlgState {
    bool result = false;
    bool closed = false;
    HWND hEditIp = nullptr, hEditToken = nullptr, hEditName = nullptr, hEditInterval = nullptr;
    HWND hCheckRecord = nullptr, hCheckLabel = nullptr, hCheckUnit = nullptr;
    HWND hComboDecimal = nullptr, hStaticStatus = nullptr;
    HWND hBtnTest = nullptr, hBtnClearHistory = nullptr;
};

static int DeleteHistoryFiles(const std::wstring& historyDirectory) {
    if (historyDirectory.empty()) {
        return 0;
    }

    std::filesystem::path directoryPath(historyDirectory);
    std::error_code ec;
    int deletedCount = 0;

    if (!std::filesystem::exists(directoryPath, ec) || !std::filesystem::is_directory(directoryPath, ec)) {
        return deletedCount;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        if (entry.path().extension() != L".csv") {
            continue;
        }

        std::error_code removeEc;
        if (std::filesystem::remove(entry.path(), removeEc)) {
            ++deletedCount;
        }
    }

    return deletedCount;
}

static void CreateControls(HWND hWnd, DlgState* st) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    
    // 创建更大的字体 - 14pt 便于阅读
    LOGFONTW lf = {};
    lf.lfHeight = -14;  // 14pt
    lf.lfWeight = FW_NORMAL;
    wcscpy_s(lf.lfFaceName, L"微软雅黑");
    HFONT hFont = CreateFontIndirectW(&lf);
    if (!hFont) hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    auto addCtrl = [&](LPCWSTR cls, LPCWSTR text, DWORD style,
                       int x, int y, int w, int h, int id) -> HWND {
        HWND hw = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                  x, y, w, h, hWnd, (HMENU)(intptr_t)id, hInst, NULL);
        if (hw && hFont) SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, TRUE);
        return hw;
    };

    // ─── 设备设置分组 ───
    addCtrl(L"BUTTON", L"设备设置", BS_GROUPBOX, 15, 15, 470, 160, 0);
    addCtrl(L"STATIC", L"设备 IP:", 0, 30, 42, 80, 24, 0);
    st->hEditIp = addCtrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 115, 38, 350, 28, IDC_EDIT_IP);

    addCtrl(L"STATIC", L"Token:", 0, 30, 80, 80, 24, 0);
    st->hEditToken = addCtrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 115, 76, 350, 28, IDC_EDIT_TOKEN);

    addCtrl(L"STATIC", L"名称:", 0, 30, 118, 80, 24, 0);
    st->hEditName = addCtrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 115, 114, 220, 28, IDC_EDIT_NAME);
    st->hBtnTest  = addCtrl(L"BUTTON", L"测试连接", BS_PUSHBUTTON, 345, 114, 120, 28, IDC_BTN_TEST);

    st->hStaticStatus = addCtrl(L"STATIC", L"", SS_LEFT, 30, 148, 420, 22, IDC_STATIC_STATUS);

    // ─── 插件选项分组 ───
    addCtrl(L"BUTTON", L"插件选项", BS_GROUPBOX, 15, 185, 470, 150, 0);
    st->hCheckRecord = addCtrl(L"BUTTON", L"启用功率历史记录", BS_AUTOCHECKBOX, 30, 210, 210, 26, IDC_CHECK_RECORD);
    st->hCheckLabel  = addCtrl(L"BUTTON", L"显示\"功率\"标签", BS_AUTOCHECKBOX, 270, 210, 200, 26, IDC_CHECK_LABEL);
    st->hCheckUnit   = addCtrl(L"BUTTON", L"显示 W 单位",   BS_AUTOCHECKBOX, 30, 245, 210, 26, IDC_CHECK_UNIT);

    addCtrl(L"STATIC", L"采集间隔（秒）:", 0, 30, 285, 130, 24, 0);
    st->hEditInterval = addCtrl(L"EDIT", L"3", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
                                160, 282, 70, 28, IDC_EDIT_INTERVAL);

    addCtrl(L"STATIC", L"小数位数:", 0, 270, 285, 100, 24, 0);
    st->hComboDecimal = addCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                                375, 282, 110, 100, IDC_COMBO_DECIMAL);
    SendMessageW(st->hComboDecimal, CB_ADDSTRING, 0, (LPARAM)L"0位");
    SendMessageW(st->hComboDecimal, CB_ADDSTRING, 0, (LPARAM)L"1位");
    SendMessageW(st->hComboDecimal, CB_ADDSTRING, 0, (LPARAM)L"2位");

    // ─── 历史数据分组 ───
    addCtrl(L"BUTTON", L"历史数据", BS_GROUPBOX, 15, 345, 470, 85, 0);
    addCtrl(L"STATIC", L"功率历史按月保存在配置目录 MijiaPower_history\\YYYY-MM.csv 中",
            SS_LEFT | SS_WORDELLIPSIS, 30, 368, 310, 40, IDC_STATIC_HISTORY);
    st->hBtnClearHistory = addCtrl(L"BUTTON", L"清除历史", BS_PUSHBUTTON, 350, 371, 120, 32, IDC_BTN_CLEARHISTORY);

    // ─── 底部按钮 ───
    addCtrl(L"BUTTON", L"确定", BS_DEFPUSHBUTTON, 270, 445, 100, 32, IDC_BTN_OK);
    addCtrl(L"BUTTON", L"取消", BS_PUSHBUTTON,    380, 445, 100, 32, IDC_BTN_CANCEL);

    // ─── 填充现有配置 ───
    auto& cfg = ConfigManager::Instance().Get();
    SetWindowTextW(st->hEditIp,    cfg.deviceIp.c_str());
    SetWindowTextW(st->hEditToken, cfg.deviceToken.c_str());
    SetWindowTextW(st->hEditName,  cfg.deviceName.c_str());

    wchar_t buf[32];
    _itow_s(cfg.updateIntervalSec, buf, 10);
    SetWindowTextW(st->hEditInterval, buf);

    SendMessageW(st->hCheckRecord, BM_SETCHECK, cfg.enableRecording ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st->hCheckLabel,  BM_SETCHECK, cfg.showLabel       ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st->hCheckUnit,   BM_SETCHECK, cfg.showUnit        ? BST_CHECKED : BST_UNCHECKED, 0);

    int dec = cfg.decimalPlaces;
    if (dec < 0) dec = 0;
    if (dec > 2) dec = 2;
    SendMessageW(st->hComboDecimal, CB_SETCURSEL, dec, 0);
}

static bool SaveFromDialog(DlgState* st) {
    auto& cfg = ConfigManager::Instance().Get();
    wchar_t buf[512];

    GetWindowTextW(st->hEditIp,    buf, 512); cfg.deviceIp    = buf;
    GetWindowTextW(st->hEditToken, buf, 512); cfg.deviceToken = buf;
    GetWindowTextW(st->hEditName,  buf, 512); cfg.deviceName  = buf;
    GetWindowTextW(st->hEditInterval, buf, 32);
    try { cfg.updateIntervalSec = std::stoi(buf); } catch (...) { cfg.updateIntervalSec = 3; }
    if (cfg.updateIntervalSec < 1) cfg.updateIntervalSec = 1;
    if (cfg.updateIntervalSec > 60) cfg.updateIntervalSec = 60;

    cfg.enableRecording = (SendMessageW(st->hCheckRecord, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.showLabel       = (SendMessageW(st->hCheckLabel,  BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.showUnit        = (SendMessageW(st->hCheckUnit,   BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.decimalPlaces   = (int)SendMessageW(st->hComboDecimal, CB_GETCURSEL, 0, 0);
    if (cfg.decimalPlaces < 0) cfg.decimalPlaces = 1;

    ConfigManager::Instance().Save();
    return true;
}

// ─── 窗口过程 ───
static LRESULT CALLBACK DlgWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 从窗口用户数据取回 DlgState*
    DlgState* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        // lParam 是 CREATESTRUCT*，其 lpCreateParams 就是我们传入的 DlgState*
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)st);
        CreateControls(hWnd, st);
        return 0;
    }

    case WM_COMMAND: {
        if (!st) break;
        int id = LOWORD(wParam);

        if (id == IDC_BTN_OK) {
            if (SaveFromDialog(st)) {
                st->result = true;
                st->closed = true;
                DestroyWindow(hWnd);
            }
        } else if (id == IDC_BTN_CANCEL) {
            st->result = false;
            st->closed = true;
            DestroyWindow(hWnd);
        } else if (id == IDC_BTN_TEST) {
            wchar_t ipBuf[256], tokenBuf[256];
            GetWindowTextW(st->hEditIp,    ipBuf,    256);
            GetWindowTextW(st->hEditToken, tokenBuf, 256);

            if (wcslen(ipBuf) == 0 || wcslen(tokenBuf) == 0) {
                SetWindowTextW(st->hStaticStatus, L"请填写 IP 和 Token");
                break;
            }
            SetWindowTextW(st->hStaticStatus, L"正在连接...");
            UpdateWindow(hWnd);

            char ip[256], token[256];
            WideCharToMultiByte(CP_UTF8, 0, ipBuf,    -1, ip,    256, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, tokenBuf, -1, token, 256, NULL, NULL);

            try {
                MiioDevice dev(ip, token, 5000);
                double watts = 0;
                if (dev.GetPower(watts)) {
                    wchar_t msg2[128];
                    swprintf_s(msg2, L"连接成功！当前功率：%.1fW", watts);
                    SetWindowTextW(st->hStaticStatus, msg2);
                } else {
                    SetWindowTextW(st->hStaticStatus, L"连接失败，请检查 IP/Token 和网络");
                }
            } catch (...) {
                SetWindowTextW(st->hStaticStatus, L"连接异常，请检查 IP/Token");
            }
        } else if (id == IDC_BTN_CLEARHISTORY) {
            if (MessageBoxW(hWnd, L"确定要清除所有功率历史记录吗？此操作不可撤销。",
                            L"确认", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                const int deletedCount = DeleteHistoryFiles(ConfigManager::Instance().GetMonthlyHistoryDirectory());
                if (deletedCount > 0) {
                    SetWindowTextW(st->hStaticStatus, L"历史记录已清除");
                } else {
                    SetWindowTextW(st->hStaticStatus, L"未找到可清除的历史记录");
                }
            }
        }
        break;
    }

    case WM_CLOSE:
        if (st) { st->result = false; st->closed = true; }
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        // 关键：绝对不能调用 PostQuitMessage！这里什么都不做
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ─── 窗口类注册（线程安全，只注册一次）───
static ATOM RegisterDlgClass(HINSTANCE hInst) {
    static ATOM atom = 0;
    if (atom == 0) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DlgWndProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"MijiaPowerOptDlg";
        wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
        atom = RegisterClassExW(&wc);
    }
    return atom;
}

// ─── 显示对话框（模态，不破坏主消息循环）───
bool COptionsDlg::Show(HWND hParent) {
    HINSTANCE hInst = hParent
        ? (HINSTANCE)GetWindowLongPtrW(hParent, GWLP_HINSTANCE)
        : GetModuleHandleW(NULL);

    RegisterDlgClass(hInst);

    DlgState state;

    // 计算居中坐标 - 宽敞的大窗口布局
    int W = 520, H = 540;  
    int px = CW_USEDEFAULT, py = CW_USEDEFAULT;
    if (hParent) {
        RECT rc{};
        GetWindowRect(hParent, &rc);
        px = rc.left + (rc.right  - rc.left - W) / 2;
        py = rc.top  + (rc.bottom - rc.top  - H) / 2;
        if (px < 0) px = 0;
        if (py < 0) py = 0;
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"MijiaPowerOptDlg",
        L"米家插座功率插件 - 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        px, py, W, H,
        hParent, NULL, hInst,
        &state   // 传给 WM_CREATE 的 CREATESTRUCT::lpCreateParams
    );

    if (!hDlg) return false;

    // 禁用父窗口（实现模态效果）
    if (hParent) EnableWindow(hParent, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    // 消息循环：只处理此对话框的消息，不 PostQuitMessage
    MSG msg{};
    while (!state.closed && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(hDlg)) break;
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // 如果窗口还活着（用户关闭了消息循环之外），强制销毁
    if (IsWindow(hDlg)) DestroyWindow(hDlg);

    // 恢复父窗口
    if (hParent) {
        EnableWindow(hParent, TRUE);
        SetForegroundWindow(hParent);
    }

    return state.result;
}
