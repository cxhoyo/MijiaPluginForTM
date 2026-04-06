// MijiaPowerPlugin.cpp - 插件主类实现
#include "pch.h"
#include "MijiaPowerPlugin.h"
#include "OptionsDlg.h"
#include <sstream>
#include <iomanip>

// ═══════════════════════════════════════════════
// DLL 导出入口
// ═══════════════════════════════════════════════
static CMijiaPowerPlugin* g_pluginInstance = nullptr;

extern "C" __declspec(dllexport)
ITMPlugin* TMPluginGetInstance() {
    if (!g_pluginInstance) {
        g_pluginInstance = new CMijiaPowerPlugin();
    }
    return g_pluginInstance;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        // 注意：不要在这里 delete 插件实例。
        // DLL_PROCESS_DETACH 阶段调用 join() 会因 loader lock 导致死锁崩溃。
        if (g_pluginInstance) {
            g_pluginInstance->Shutdown();
            g_pluginInstance = nullptr;
        }
        break;
    }
    return TRUE;
}

// ═══════════════════════════════════════════════
// CPowerItem 实现
// ═══════════════════════════════════════════════
const wchar_t* CPowerItem::GetItemName() const {
    return L"米家插座功率";
}

const wchar_t* CPowerItem::GetItemLableText() const {
    auto& cfg = ConfigManager::Instance().Get();
    if (cfg.showLabel)
        m_labelText = L"功率:";
    else
        m_labelText = L"";
    return m_labelText.c_str();
}

const wchar_t* CPowerItem::GetItemValueText() const {
    if (!m_plugin) { m_valueText = L"--"; return m_valueText.c_str(); }

    if (!m_plugin->IsConnected()) {
        auto& cfg = ConfigManager::Instance().Get();
        if (cfg.deviceIp.empty())
            m_valueText = L"未配置";
        else
            m_valueText = L"连接中...";
        return m_valueText.c_str();
    }

    double watts = m_plugin->GetCurrentWatts();
    auto& cfg = ConfigManager::Instance().Get();

    std::wostringstream oss;
    oss << std::fixed << std::setprecision(cfg.decimalPlaces) << watts;
    if (cfg.showUnit) oss << L"W";
    m_valueText = oss.str();
    return m_valueText.c_str();
}

// ═══════════════════════════════════════════════
// CMijiaPowerPlugin 实现
// ═══════════════════════════════════════════════
CMijiaPowerPlugin::CMijiaPowerPlugin() {
    m_powerItem.SetPlugin(this);
}

CMijiaPowerPlugin::~CMijiaPowerPlugin() {
    // 析构在普通线程上调用（不在 loader lock），join 是安全的
    m_stopFlag = true;
    if (m_sampleThread.joinable())
        m_sampleThread.join();
}

// API v7：主程序在加载插件后调用此函数，传入 ITrafficMonitor*
void CMijiaPowerPlugin::OnInitialize(ITrafficMonitor* pApp) {
    m_pTM = pApp;
    // 注意：此时配置目录可能还未通过 OnExtenedInfo 传入。
    // 不在这里加载配置，等 OnExtenedInfo(EI_CONFIG_DIR) 时再初始化。
    // 但为了兼容老版本（未调用 OnInitialize），我们在 StartSampling 里做懒加载。
}

// 主程序通过此函数传入扩展信息，包括配置目录（EI_CONFIG_DIR）
void CMijiaPowerPlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) {
    if (index == EI_CONFIG_DIR && data && data[0] != L'\0') {
        if (!m_initialized) {
            m_initialized = true;
            ConfigManager::Instance().SetConfigDir(data);
            ConfigManager::Instance().Load();
            auto& cfg = ConfigManager::Instance().Get();
            if (cfg.enableRecording) {
                m_history.LoadFromFile(ConfigManager::Instance().GetMonthlyHistoryDirectory());
            }

            // 启动采样线程
            StartSampling();
        }
    }
}

void CMijiaPowerPlugin::StartSampling() {
    if (!m_sampleThread.joinable()) {
        m_sampleThread = std::thread(&CMijiaPowerPlugin::SampleLoop, this);
    }
}

void CMijiaPowerPlugin::SampleLoop() {
    // 首次连接
    ConnectDevice();

    int elapsed = 0;
    ULONGLONG lastHistorySaveTick = GetTickCount64();
    while (!m_stopFlag) {
        Sleep(1000);
        elapsed++;

        auto& cfg = ConfigManager::Instance().Get();
        int interval = cfg.updateIntervalSec;
        if (interval < 1) interval = 1;

        if (elapsed >= interval) {
            elapsed = 0;

            // 尝试读取功率
            std::lock_guard<std::mutex> lock(m_deviceMutex);
            if (!m_device) {
                // 尝试重连
                if (!cfg.deviceIp.empty() && !cfg.deviceToken.empty()) {
                    char ip[256], token[256];
                    WideCharToMultiByte(CP_ACP, 0, cfg.deviceIp.c_str(),    -1, ip,    256, NULL, NULL);
                    WideCharToMultiByte(CP_ACP, 0, cfg.deviceToken.c_str(), -1, token, 256, NULL, NULL);
                    try {
                        auto dev = std::make_unique<MiioDevice>(ip, token, 5000);
                        double w = 0;
                        if (dev->GetPower(w)) {
                            m_device   = std::move(dev);
                            m_connected = true;
                            m_currentWatts = w;
                            if (cfg.enableRecording)
                                m_history.AddSample(w);
                        }
                    } catch (...) {}
                }
            } else {
                // 已连接，读取数据
                double w = 0;
                if (m_device->GetPower(w)) {
                    m_connected    = true;
                    m_currentWatts = w;
                    if (cfg.enableRecording)
                        m_history.AddSample(w);
                } else {
                    // 连接失效，重置
                    m_device.reset();
                    m_connected = false;
                }
            }
        }

        // 定期落盘，避免依赖 DLL 卸载时机导致历史文件丢失。
        if (cfg.enableRecording && m_history.HasData()) {
            ULONGLONG now = GetTickCount64();
            if (now - lastHistorySaveTick >= 60000) {
                PersistHistoryIfEnabled();
                lastHistorySaveTick = now;
            }
        }
    }

}

void CMijiaPowerPlugin::PersistHistoryIfEnabled() const {
    if (!m_initialized) {
        return;
    }

    const auto& cfg = ConfigManager::Instance().Get();
    if (!cfg.enableRecording) {
        return;
    }

    auto historyPath = ConfigManager::Instance().GetMonthlyHistoryDirectory();
    if (historyPath.empty()) {
        return;
    }

    m_history.SaveToFile(historyPath);
}

void CMijiaPowerPlugin::ConnectDevice() {
    auto& cfg = ConfigManager::Instance().Get();
    if (cfg.deviceIp.empty() || cfg.deviceToken.empty()) return;

    char ip[256], token[256];
    WideCharToMultiByte(CP_ACP, 0, cfg.deviceIp.c_str(),    -1, ip,    256, NULL, NULL);
    WideCharToMultiByte(CP_ACP, 0, cfg.deviceToken.c_str(), -1, token, 256, NULL, NULL);

    try {
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        auto dev = std::make_unique<MiioDevice>(ip, token, 5000);
        double w = 0;
        if (dev->GetPower(w)) {
            m_device   = std::move(dev);
            m_connected = true;
            m_currentWatts = w;
        }
    } catch (...) {}
}

void CMijiaPowerPlugin::DisconnectDevice() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_device.reset();
    m_connected = false;
}

double CMijiaPowerPlugin::GetCurrentWatts() const {
    return m_currentWatts.load();
}

// ─── ITMPlugin 接口 ───
IPluginItem* CMijiaPowerPlugin::GetItem(int index) {
    if (index == 0) return &m_powerItem;
    return nullptr;
}

void CMijiaPowerPlugin::DataRequired() {
    // TrafficMonitor 定期调用此函数刷新数据
    // 数据由后台线程持续更新，无需在此处理
    // 但如果配置目录还未通过 OnExtenedInfo 传来，尝试用当前目录初始化（兼容性）
    if (!m_initialized) {
        m_initialized = true;
        wchar_t buf[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, buf);
        ConfigManager::Instance().SetConfigDir(buf);
        ConfigManager::Instance().Load();
        auto& cfg = ConfigManager::Instance().Get();
        if (cfg.enableRecording) {
            m_history.LoadFromFile(ConfigManager::Instance().GetMonthlyHistoryDirectory());
        }
        StartSampling();
    }
}

const wchar_t* CMijiaPowerPlugin::GetInfo(PluginInfoIndex index) {
    switch (index) {
    case TMI_NAME:        return L"米家插座功率";
    case TMI_DESCRIPTION: return L"实时显示米家/酷控智能插座的功率，支持历史记录";
    case TMI_AUTHOR:      return L"MijiaPlug";
    case TMI_COPYRIGHT:   return L"2024 MijiaPlug";
    case TMI_URL:         return L"";
    case TMI_VERSION:     return L"1.0.0";
    default:              return L"";
    }
}

// 返回值改为 OptionReturn
ITMPlugin::OptionReturn CMijiaPowerPlugin::ShowOptionsDialog(void* hParent) {
    bool changed = COptionsDlg::Show((HWND)hParent);
    if (changed) {
        // 配置已更新，重新连接设备
        DisconnectDevice();
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* CMijiaPowerPlugin::GetTooltipInfo() {
    auto& cfg = ConfigManager::Instance().Get();

    std::wostringstream oss;
    oss << L"【" << cfg.deviceName << L"】";
    if (!m_connected) {
        oss << L"\n状态：未连接";
        if (!cfg.deviceIp.empty())
            oss << L"\nIP：" << cfg.deviceIp;
    } else {
        double w = m_currentWatts.load();
        oss << std::fixed << std::setprecision(1);
        oss << L"\n当前功率：" << w << L" W";

        if (cfg.enableRecording) {
            auto st10m = m_history.GetStats(600);
            if (st10m.valid) {
                oss << L"\n--- 最近10分钟 ---"
                    << L"\n  最大：" << st10m.maxW << L" W"
                    << L"\n  最小：" << st10m.minW << L" W"
                    << L"\n  平均：" << st10m.avgW << L" W";
            }
            auto st1h = m_history.GetLongStats(1);
            if (st1h.valid) {
                oss << L"\n--- 最近1小时 ---"
                    << L"\n  最大：" << st1h.maxW << L" W"
                    << L"\n  最小：" << st1h.minW << L" W"
                    << L"\n  平均：" << st1h.avgW << L" W";
            }
            auto st24h = m_history.GetLongStats(24);
            if (st24h.valid) {
                oss << L"\n--- 最近24小时 ---"
                    << L"\n  最大：" << st24h.maxW << L" W"
                    << L"\n  最小：" << st24h.minW << L" W"
                    << L"\n  平均：" << st24h.avgW << L" W";
            }
        }
    }

    m_tooltipText = oss.str();
    return m_tooltipText.c_str();
}
