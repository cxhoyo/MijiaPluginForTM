// MijiaPowerPlugin.cpp - multi-device TrafficMonitor plugin
#include "pch.h"
#include "MijiaPowerPlugin.h"
#include "OptionsDlg.h"

static CMijiaPowerPlugin* g_pluginInstance = nullptr;

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    if (!g_pluginInstance) g_pluginInstance = new CMijiaPowerPlugin();
    return g_pluginInstance;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    if (reason == DLL_PROCESS_DETACH && g_pluginInstance) {
        g_pluginInstance->Shutdown();
        g_pluginInstance = nullptr;
    }
    return TRUE;
}

CPowerItem::CPowerItem(CMijiaPowerPlugin* plugin, std::shared_ptr<DeviceRuntime> runtime)
    : m_plugin(plugin), m_runtime(std::move(runtime)) {}

std::wstring CPowerItem::FormatLabel() const {
    const auto& settings = ConfigManager::Instance().Get();
    if (!settings.showLabel) return {};
    return m_runtime->GetConfig().deviceName + L"：";
}

std::wstring CPowerItem::FormatValue(bool sample) const {
    const auto& settings = ConfigManager::Instance().Get();
    if (sample) {
        std::wstring value = (settings.decimalPlaces == 0) ? L"9999" :
            (settings.decimalPlaces == 1 ? L"9999.9" : L"9999.99");
        if (settings.showUnit) value += L"W";
        return value;
    }
    if (m_runtime->IsPendingRemoval()) return L"待重启移除";
    if (!m_runtime->IsConnected()) return L"连接中...";

    std::wostringstream value;
    value << std::fixed << std::setprecision(settings.decimalPlaces)
          << m_runtime->GetCurrentWatts();
    if (settings.showUnit) value << L"W";
    return value.str();
}

const wchar_t* CPowerItem::GetItemName() const {
    m_itemName = m_runtime->GetConfig().deviceName;
    return m_itemName.c_str();
}

const wchar_t* CPowerItem::GetItemId() const {
    m_itemId = m_runtime->GetConfig().itemId;
    return m_itemId.c_str();
}

const wchar_t* CPowerItem::GetItemLableText() const {
    m_labelText = FormatLabel();
    return m_labelText.c_str();
}

const wchar_t* CPowerItem::GetItemValueText() const {
    m_valueText = FormatValue(false);
    return m_valueText.c_str();
}

const wchar_t* CPowerItem::GetItemValueSampleText() const {
    m_sampleText = FormatValue(true);
    return m_sampleText.c_str();
}

int CPowerItem::GetItemWidth() const {
    const auto sample = FormatLabel() + FormatValue(true);
    return (std::max)(48, static_cast<int>(sample.size()) * 9 + 4);
}

int CPowerItem::GetItemWidthEx(void* hDC) const {
    if (!hDC) return GetItemWidth();
    HDC dc = static_cast<HDC>(hDC);
    const auto sample = FormatLabel() + FormatValue(true);
    SIZE size{};
    if (!GetTextExtentPoint32W(dc, sample.c_str(), static_cast<int>(sample.size()), &size)) {
        return GetItemWidth();
    }
    return size.cx + 4;
}

void CPowerItem::DrawItem(void* hDC, int x, int y, int w, int h, bool) {
    if (!hDC || w <= 0 || h <= 0) return;
    HDC dc = static_cast<HDC>(hDC);
    const auto label = FormatLabel();
    const auto value = FormatValue(false);

    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    int drawHeight = h;
    if (m_plugin->IsDrawingTaskbar() && metrics.tmHeight > 0 && h > metrics.tmHeight * 3 / 2) {
        drawHeight = h / 2;
    }

    SetBkMode(dc, TRANSPARENT);
    RECT labelRect{ x + 2, y, x + w, y + drawHeight };
    if (!label.empty()) {
        SetTextColor(dc, m_plugin->LabelColor());
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &labelRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SIZE labelSize{};
        GetTextExtentPoint32W(dc, label.c_str(), static_cast<int>(label.size()), &labelSize);
        labelRect.left += labelSize.cx;
    }

    SetTextColor(dc, m_plugin->ValueColor());
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &labelRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

CMijiaPowerPlugin::~CMijiaPowerPlugin() {
    for (auto& runtime : m_runtimes) runtime->Stop(true);
}

void CMijiaPowerPlugin::Initialize(const std::wstring& configDir) {
    if (m_initialized) return;
    m_initialized = true;
    ConfigManager::Instance().SetConfigDir(configDir);
    ConfigManager::Instance().Load();

    for (const auto& device : ConfigManager::Instance().Get().devices) {
        auto runtime = std::make_shared<DeviceRuntime>(device);
        m_items.push_back(std::make_unique<CPowerItem>(this, runtime));
        m_runtimes.push_back(std::move(runtime));
    }
    StartRuntimes();
}

void CMijiaPowerPlugin::StartRuntimes() {
    const auto& settings = ConfigManager::Instance().Get();
    for (auto& runtime : m_runtimes) {
        runtime->SetSharedSettings(settings.updateIntervalSec, settings.enableRecording);
        runtime->Start();
    }
}

void CMijiaPowerPlugin::ApplyConfigurationToRuntimes() {
    const auto& settings = ConfigManager::Instance().Get();
    std::map<std::wstring, DeviceConfig> configured;
    for (const auto& device : settings.devices) configured[device.itemId] = device;

    for (auto& runtime : m_runtimes) {
        const auto current = runtime->GetConfig();
        runtime->SetSharedSettings(settings.updateIntervalSec, settings.enableRecording);
        auto found = configured.find(current.itemId);
        if (found == configured.end()) runtime->MarkPendingRemoval();
        else runtime->UpdateConfig(found->second);
    }
}

void CMijiaPowerPlugin::Shutdown() {
    for (auto& runtime : m_runtimes) runtime->Stop(false);
}

void CMijiaPowerPlugin::OnInitialize(ITrafficMonitor* app) {
    m_pTM = app;
}

void CMijiaPowerPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    if (index == EI_CONFIG_DIR && data && data[0] != L'\0') {
        Initialize(data);
    } else if (data && index == EI_LABEL_TEXT_COLOR) {
        m_labelColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
    } else if (data && index == EI_VALUE_TEXT_COLOR) {
        m_valueColor = static_cast<COLORREF>(wcstoul(data, nullptr, 10));
    } else if (data && index == EI_DRAW_TASKBAR_WND) {
        m_drawingTaskbar = wcscmp(data, L"1") == 0;
    }
}

IPluginItem* CMijiaPowerPlugin::GetItem(int index) {
    if (index < 0 || static_cast<size_t>(index) >= m_items.size()) return nullptr;
    return m_items[static_cast<size_t>(index)].get();
}

void CMijiaPowerPlugin::DataRequired() {
    if (!m_initialized) {
        wchar_t currentDir[MAX_PATH] = {};
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        Initialize(currentDir);
    }
}

const wchar_t* CMijiaPowerPlugin::GetInfo(PluginInfoIndex index) {
    switch (index) {
    case TMI_NAME: return L"米家多设备功率";
    case TMI_DESCRIPTION: return L"独立显示多台米家/酷控智能插座的实时功率和历史统计";
    case TMI_AUTHOR: return L"MijiaPlug contributors";
    case TMI_COPYRIGHT: return L"2024-2026 MijiaPlug contributors";
    case TMI_URL: return L"https://github.com/cxhoyo/MijiaPluginForTM";
    case TMI_VERSION: return L"2.0.0";
    default: return L"";
    }
}

ITMPlugin::OptionReturn CMijiaPowerPlugin::ShowOptionsDialog(void* hParent) {
    if (!COptionsDlg::Show(static_cast<HWND>(hParent))) return OR_OPTION_UNCHANGED;
    ApplyConfigurationToRuntimes();
    return OR_OPTION_CHANGED;
}

const wchar_t* CMijiaPowerPlugin::GetTooltipInfo() {
    const auto& settings = ConfigManager::Instance().Get();
    std::wostringstream tooltip;

    for (size_t i = 0; i < m_runtimes.size(); ++i) {
        const auto& runtime = m_runtimes[i];
        const auto device = runtime->GetConfig();
        if (i > 0) tooltip << L"\n";
        tooltip << L"【" << device.deviceName << L"】";

        if (runtime->IsPendingRemoval()) {
            tooltip << L" 待重启移除";
        } else if (!runtime->IsConnected()) {
            tooltip << L" 未连接";
        } else {
            tooltip << L" " << std::fixed << std::setprecision(settings.decimalPlaces)
                    << runtime->GetCurrentWatts() << L" W";
            if (settings.enableRecording) {
                const auto stats = runtime->GetHistory().GetLongStats(1);
                if (stats.valid) {
                    tooltip << L"\n最近1小时  最低 " << stats.minW << L" W  平均 "
                            << stats.avgW << L" W  最高 " << stats.maxW << L" W";
                }
            }
        }
    }
    if (m_runtimes.empty()) tooltip << L"尚未配置设备，请打开插件选项添加。";
    m_tooltipText = tooltip.str();
    return m_tooltipText.c_str();
}
