// OptionsDlg.cpp - multi-device Win32 settings dialog
#include "pch.h"
#include "OptionsDlg.h"
#include "PluginConfig.h"
#include "MiioDevice.h"
#include <commctrl.h>

namespace {
constexpr int IDC_DEVICE_LIST = 1001;
constexpr int IDC_ADD_DEVICE = 1002;
constexpr int IDC_DELETE_DEVICE = 1003;
constexpr int IDC_MOVE_UP = 1004;
constexpr int IDC_MOVE_DOWN = 1005;
constexpr int IDC_EDIT_NAME = 1010;
constexpr int IDC_EDIT_IP = 1011;
constexpr int IDC_EDIT_TOKEN = 1012;
constexpr int IDC_TEST_DEVICE = 1013;
constexpr int IDC_DEVICE_STATUS = 1014;
constexpr int IDC_CHECK_RECORD = 1020;
constexpr int IDC_CHECK_LABEL = 1021;
constexpr int IDC_CHECK_UNIT = 1022;
constexpr int IDC_EDIT_INTERVAL = 1023;
constexpr int IDC_COMBO_DECIMAL = 1024;
constexpr int IDC_CLEAR_SELECTED_HISTORY = 1030;
constexpr int IDC_CLEAR_ALL_HISTORY = 1031;
constexpr int IDC_SAVE = 1040;
constexpr int IDC_CANCEL = 1041;

struct DialogState {
    bool result = false;
    bool closed = false;
    bool updatingList = false;
    int selectedIndex = -1;
    PluginConfig working;
    std::vector<std::wstring> originalItemOrder;

    HWND list = nullptr;
    HWND name = nullptr;
    HWND ip = nullptr;
    HWND token = nullptr;
    HWND status = nullptr;
    HWND checkRecord = nullptr;
    HWND checkLabel = nullptr;
    HWND checkUnit = nullptr;
    HWND interval = nullptr;
    HWND decimals = nullptr;
    HFONT font = nullptr;
};

HWND AddControl(HWND parent, LPCWSTR cls, LPCWSTR text, DWORD style,
                int x, int y, int width, int height, int id, HFONT font,
                DWORD exStyle = 0) {
    HWND control = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
    if (control && font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}

std::wstring GetControlText(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

void SetEditorEnabled(DialogState* state, bool enabled) {
    EnableWindow(state->name, enabled);
    EnableWindow(state->ip, enabled);
    EnableWindow(state->token, enabled);
    EnableWindow(GetDlgItem(GetParent(state->list), IDC_TEST_DEVICE), enabled);
    EnableWindow(GetDlgItem(GetParent(state->list), IDC_DELETE_DEVICE), enabled);
    EnableWindow(GetDlgItem(GetParent(state->list), IDC_MOVE_UP),
                 enabled && state->selectedIndex > 0);
    EnableWindow(GetDlgItem(GetParent(state->list), IDC_MOVE_DOWN),
                 enabled && state->selectedIndex + 1 < static_cast<int>(state->working.devices.size()));
}

void SaveSelectedEdits(DialogState* state) {
    if (state->selectedIndex < 0 ||
        state->selectedIndex >= static_cast<int>(state->working.devices.size())) return;
    auto& device = state->working.devices[static_cast<size_t>(state->selectedIndex)];
    device.deviceName = GetControlText(state->name);
    device.deviceIp = GetControlText(state->ip);
    device.deviceToken = GetControlText(state->token);
}

void LoadSelectedEdits(DialogState* state) {
    const bool valid = state->selectedIndex >= 0 &&
        state->selectedIndex < static_cast<int>(state->working.devices.size());
    SetEditorEnabled(state, valid);
    if (!valid) {
        SetWindowTextW(state->name, L"");
        SetWindowTextW(state->ip, L"");
        SetWindowTextW(state->token, L"");
        SetWindowTextW(state->status, L"请添加或选择设备");
        return;
    }

    const auto& device = state->working.devices[static_cast<size_t>(state->selectedIndex)];
    SetWindowTextW(state->name, device.deviceName.c_str());
    SetWindowTextW(state->ip, device.deviceIp.c_str());
    SetWindowTextW(state->token, device.deviceToken.c_str());
    SetWindowTextW(state->status, L"");
}

void RefreshDeviceList(DialogState* state, int selectIndex) {
    state->updatingList = true;
    ListView_DeleteAllItems(state->list);
    for (size_t i = 0; i < state->working.devices.size(); ++i) {
        const auto& device = state->working.devices[i];
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<wchar_t*>(device.deviceName.c_str());
        ListView_InsertItem(state->list, &item);
        ListView_SetItemText(state->list, static_cast<int>(i), 1,
            const_cast<wchar_t*>(device.deviceIp.c_str()));
    }

    if (!state->working.devices.empty()) {
        selectIndex = (std::max)(0, (std::min)(selectIndex,
            static_cast<int>(state->working.devices.size()) - 1));
        state->selectedIndex = selectIndex;
        ListView_SetItemState(state->list, selectIndex, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(state->list, selectIndex, FALSE);
    } else {
        state->selectedIndex = -1;
    }
    state->updatingList = false;
    LoadSelectedEdits(state);
}

std::vector<std::wstring> ItemOrder(const PluginConfig& config) {
    std::vector<std::wstring> result;
    for (const auto& device : config.devices) result.push_back(device.itemId);
    return result;
}

void CreateControls(HWND window, DialogState* state) {
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&controls);

    LOGFONTW fontInfo{};
    fontInfo.lfHeight = -16;
    wcscpy_s(fontInfo.lfFaceName, L"Microsoft YaHei UI");
    HFONT font = CreateFontIndirectW(&fontInfo);
    if (!font) font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    state->font = font;

    AddControl(window, L"BUTTON", L"设备列表（显示顺序：上行、下行、向右）", BS_GROUPBOX,
               12, 10, 350, 420, 0, font);
    state->list = AddControl(window, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                             24, 38, 326, 300, IDC_DEVICE_LIST, font, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(state->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = 145;
    column.pszText = const_cast<wchar_t*>(L"设备名称");
    ListView_InsertColumn(state->list, 0, &column);
    column.cx = 155;
    column.pszText = const_cast<wchar_t*>(L"IP 地址");
    ListView_InsertColumn(state->list, 1, &column);

    AddControl(window, L"BUTTON", L"添加", BS_PUSHBUTTON, 24, 350, 72, 30, IDC_ADD_DEVICE, font);
    AddControl(window, L"BUTTON", L"删除", BS_PUSHBUTTON, 104, 350, 72, 30, IDC_DELETE_DEVICE, font);
    AddControl(window, L"BUTTON", L"上移", BS_PUSHBUTTON, 184, 350, 72, 30, IDC_MOVE_UP, font);
    AddControl(window, L"BUTTON", L"下移", BS_PUSHBUTTON, 264, 350, 72, 30, IDC_MOVE_DOWN, font);
    AddControl(window, L"STATIC", L"设备增删或排序后需重启 TrafficMonitor 生效。", SS_LEFT,
               24, 392, 320, 24, 0, font);

    AddControl(window, L"BUTTON", L"选中设备", BS_GROUPBOX, 374, 10, 394, 250, 0, font);
    AddControl(window, L"STATIC", L"名称:", 0, 392, 42, 62, 24, 0, font);
    state->name = AddControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                             462, 38, 286, 28, IDC_EDIT_NAME, font);
    AddControl(window, L"STATIC", L"IP:", 0, 392, 82, 62, 24, 0, font);
    state->ip = AddControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                           462, 78, 286, 28, IDC_EDIT_IP, font);
    AddControl(window, L"STATIC", L"Token:", 0, 392, 122, 62, 24, 0, font);
    state->token = AddControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                              462, 118, 286, 28, IDC_EDIT_TOKEN, font);
    SendMessageW(state->token, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'●'), 0);
    AddControl(window, L"BUTTON", L"测试连接", BS_PUSHBUTTON,
               628, 160, 120, 30, IDC_TEST_DEVICE, font);
    state->status = AddControl(window, L"STATIC", L"", SS_LEFT,
                               392, 202, 356, 42, IDC_DEVICE_STATUS, font);

    AddControl(window, L"BUTTON", L"共享显示与采样设置", BS_GROUPBOX,
               374, 270, 394, 160, 0, font);
    state->checkRecord = AddControl(window, L"BUTTON", L"记录每台设备的功率历史", BS_AUTOCHECKBOX,
                                    392, 298, 250, 26, IDC_CHECK_RECORD, font);
    state->checkLabel = AddControl(window, L"BUTTON", L"显示设备名称标签", BS_AUTOCHECKBOX,
                                   392, 330, 180, 26, IDC_CHECK_LABEL, font);
    state->checkUnit = AddControl(window, L"BUTTON", L"显示 W 单位", BS_AUTOCHECKBOX,
                                  580, 330, 150, 26, IDC_CHECK_UNIT, font);
    AddControl(window, L"STATIC", L"采集间隔（秒）:", 0, 392, 372, 130, 24, 0, font);
    state->interval = AddControl(window, L"EDIT", L"3", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
                                 522, 368, 60, 28, IDC_EDIT_INTERVAL, font);
    AddControl(window, L"STATIC", L"小数位:", 0, 604, 372, 72, 24, 0, font);
    state->decimals = AddControl(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                                 678, 368, 70, 100, IDC_COMBO_DECIMAL, font);
    SendMessageW(state->decimals, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"0"));
    SendMessageW(state->decimals, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1"));
    SendMessageW(state->decimals, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2"));

    AddControl(window, L"BUTTON", L"历史数据", BS_GROUPBOX, 12, 442, 756, 74, 0, font);
    AddControl(window, L"BUTTON", L"清除选中设备历史", BS_PUSHBUTTON,
               392, 468, 170, 30, IDC_CLEAR_SELECTED_HISTORY, font);
    AddControl(window, L"BUTTON", L"清除全部历史", BS_PUSHBUTTON,
               578, 468, 150, 30, IDC_CLEAR_ALL_HISTORY, font);

    AddControl(window, L"BUTTON", L"确定", BS_DEFPUSHBUTTON, 548, 532, 100, 32, IDC_SAVE, font);
    AddControl(window, L"BUTTON", L"取消", BS_PUSHBUTTON, 660, 532, 100, 32, IDC_CANCEL, font);

    state->working = ConfigManager::Instance().Get();
    state->originalItemOrder = ItemOrder(state->working);
    SendMessageW(state->checkRecord, BM_SETCHECK, state->working.enableRecording ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state->checkLabel, BM_SETCHECK, state->working.showLabel ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state->checkUnit, BM_SETCHECK, state->working.showUnit ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(state->interval, std::to_wstring(state->working.updateIntervalSec).c_str());
    SendMessageW(state->decimals, CB_SETCURSEL, state->working.decimalPlaces, 0);
    RefreshDeviceList(state, 0);
}

bool ValidateAndSave(HWND window, DialogState* state) {
    SaveSelectedEdits(state);
    for (size_t i = 0; i < state->working.devices.size(); ++i) {
        auto& device = state->working.devices[i];
        if (device.deviceName.empty() || device.deviceIp.empty() || device.deviceToken.empty()) {
            RefreshDeviceList(state, static_cast<int>(i));
            MessageBoxW(window, L"每台设备都必须填写名称、IP 和 Token。", L"配置不完整",
                        MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    state->working.enableRecording = SendMessageW(state->checkRecord, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state->working.showLabel = SendMessageW(state->checkLabel, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state->working.showUnit = SendMessageW(state->checkUnit, BM_GETCHECK, 0, 0) == BST_CHECKED;
    try {
        state->working.updateIntervalSec = std::stoi(GetControlText(state->interval));
    } catch (...) {
        state->working.updateIntervalSec = 3;
    }
    state->working.updateIntervalSec = (std::max)(1, (std::min)(60, state->working.updateIntervalSec));
    state->working.decimalPlaces = static_cast<int>(SendMessageW(state->decimals, CB_GETCURSEL, 0, 0));
    if (state->working.decimalPlaces < 0) state->working.decimalPlaces = 1;

    ConfigManager::Instance().Get() = state->working;
    if (!ConfigManager::Instance().Save()) {
        MessageBoxW(window, L"无法保存 MijiaPower.ini，请检查配置目录权限。", L"保存失败",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    if (state->originalItemOrder != ItemOrder(state->working)) {
        MessageBoxW(window, L"设备集合或顺序已改变。请重启 TrafficMonitor 使显示项变化生效。",
                    L"需要重启", MB_OK | MB_ICONINFORMATION);
    }
    return true;
}

void TestSelectedDevice(HWND window, DialogState* state) {
    SaveSelectedEdits(state);
    if (state->selectedIndex < 0) return;
    const auto& device = state->working.devices[static_cast<size_t>(state->selectedIndex)];
    if (device.deviceIp.empty() || device.deviceToken.empty()) {
        SetWindowTextW(state->status, L"请先填写 IP 和 Token。");
        return;
    }

    SetWindowTextW(state->status, L"正在连接...");
    UpdateWindow(window);
    auto toUtf8 = [](const std::wstring& value) {
        int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>((std::max)(0, length)), '\0');
        if (length > 0) WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                            result.data(), length, nullptr, nullptr);
        return result;
    };

    try {
        MiioDevice test(toUtf8(device.deviceIp), toUtf8(device.deviceToken), 5000);
        double watts = 0;
        if (test.GetPower(watts)) {
            wchar_t result[128] = {};
            swprintf_s(result, L"连接成功，当前功率 %.1f W", watts);
            SetWindowTextW(state->status, result);
        } else {
            SetWindowTextW(state->status, L"连接失败，请检查 IP、Token 和局域网。");
        }
    } catch (...) {
        SetWindowTextW(state->status, L"连接异常，请检查设备配置。");
    }
}

void ClearHistory(HWND window, DialogState* state, bool all) {
    SaveSelectedEdits(state);
    if (!all && state->selectedIndex < 0) return;
    const wchar_t* prompt = all ? L"确定清除当前配置中全部设备的功率历史吗？此操作不可撤销。"
                                : L"确定清除选中设备的功率历史吗？此操作不可撤销。";
    if (MessageBoxW(window, prompt, L"确认清除历史", MB_YESNO | MB_ICONWARNING) != IDYES) return;

    if (all) {
        for (const auto& device : state->working.devices) {
            DeleteFileW(ConfigManager::Instance().GetHistoryFilePath(device.itemId).c_str());
        }
        DeleteFileW(ConfigManager::Instance().GetLegacyHistoryFilePath().c_str());
    } else {
        const auto& device = state->working.devices[static_cast<size_t>(state->selectedIndex)];
        DeleteFileW(ConfigManager::Instance().GetHistoryFilePath(device.itemId).c_str());
        if (device.itemId == L"MijiaPowerW") {
            DeleteFileW(ConfigManager::Instance().GetLegacyHistoryFilePath().c_str());
        }
    }
    SetWindowTextW(state->status, L"历史记录已清除。");
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<DialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        CreateControls(window, state);
        return 0;
    }

    if (message == WM_NOTIFY && state) {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->idFrom == IDC_DEVICE_LIST && header->code == LVN_ITEMCHANGED && !state->updatingList) {
            auto* changed = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((changed->uNewState & LVIS_SELECTED) && !(changed->uOldState & LVIS_SELECTED)) {
                SaveSelectedEdits(state);
                state->selectedIndex = changed->iItem;
                LoadSelectedEdits(state);
            }
        }
        return 0;
    }

    if (message == WM_COMMAND && state) {
        const int id = LOWORD(wParam);
        if (id == IDC_ADD_DEVICE) {
            SaveSelectedEdits(state);
            DeviceConfig device;
            device.itemId = state->working.devices.empty() ? L"MijiaPowerW"
                                                            : ConfigManager::Instance().CreateItemId();
            device.deviceName = L"新设备";
            state->working.devices.push_back(std::move(device));
            RefreshDeviceList(state, static_cast<int>(state->working.devices.size()) - 1);
        } else if (id == IDC_DELETE_DEVICE && state->selectedIndex >= 0) {
            if (MessageBoxW(window, L"删除设备配置？历史文件不会自动删除。", L"确认删除",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                const int oldIndex = state->selectedIndex;
                state->working.devices.erase(state->working.devices.begin() + oldIndex);
                RefreshDeviceList(state, oldIndex);
            }
        } else if (id == IDC_MOVE_UP && state->selectedIndex > 0) {
            SaveSelectedEdits(state);
            const int index = state->selectedIndex;
            std::swap(state->working.devices[index], state->working.devices[index - 1]);
            RefreshDeviceList(state, index - 1);
        } else if (id == IDC_MOVE_DOWN && state->selectedIndex >= 0 &&
                   state->selectedIndex + 1 < static_cast<int>(state->working.devices.size())) {
            SaveSelectedEdits(state);
            const int index = state->selectedIndex;
            std::swap(state->working.devices[index], state->working.devices[index + 1]);
            RefreshDeviceList(state, index + 1);
        } else if (id == IDC_TEST_DEVICE) {
            TestSelectedDevice(window, state);
        } else if (id == IDC_CLEAR_SELECTED_HISTORY) {
            ClearHistory(window, state, false);
        } else if (id == IDC_CLEAR_ALL_HISTORY) {
            ClearHistory(window, state, true);
        } else if (id == IDC_SAVE) {
            if (ValidateAndSave(window, state)) {
                state->result = true;
                state->closed = true;
                DestroyWindow(window);
            }
        } else if (id == IDC_CANCEL) {
            state->closed = true;
            DestroyWindow(window);
        }
        return 0;
    }

    if (message == WM_CLOSE) {
        if (state) state->closed = true;
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY && state && state->font &&
        state->font != GetStockObject(DEFAULT_GUI_FONT)) {
        DeleteObject(state->font);
        state->font = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

ATOM RegisterWindowClass(HINSTANCE instance) {
    static ATOM atom = 0;
    if (atom != 0) return atom;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = L"MijiaPowerMultiDeviceOptions";
    atom = RegisterClassExW(&windowClass);
    return atom;
}
}

bool COptionsDlg::Show(HWND parent) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!RegisterWindowClass(instance)) return false;

    DialogState state;
    constexpr int width = 800;
    constexpr int height = 620;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (parent) {
        RECT parentRect{};
        GetWindowRect(parent, &parentRect);
        x = static_cast<int>((std::max)(0L,
            parentRect.left + (parentRect.right - parentRect.left - width) / 2));
        y = static_cast<int>((std::max)(0L,
            parentRect.top + (parentRect.bottom - parentRect.top - height) / 2));
    }

    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"MijiaPowerMultiDeviceOptions", L"米家多设备功率插件 - 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x, y, width, height,
        parent, nullptr, instance, &state);
    if (!window) return false;

    if (parent) EnableWindow(parent, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message{};
    while (!state.closed && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsWindow(window)) break;
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    if (parent) {
        EnableWindow(parent, TRUE);
        SetForegroundWindow(parent);
    }
    return state.result;
}
