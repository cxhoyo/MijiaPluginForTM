// MijiaPowerPlugin.h - 插件主类声明
#pragma once
#include "pch.h"
#include "PluginInterface.h"
#include "MiioDevice.h"
#include "PowerHistory.h"
#include "PluginConfig.h"

// ─────────────────────────────────────────────
// 功率显示项（主显示：实时功率）
// ─────────────────────────────────────────────
class CPowerItem : public IPluginItem {
public:
    void SetPlugin(class CMijiaPowerPlugin* plugin) { m_plugin = plugin; }

    const wchar_t* GetItemName()            const override;
    const wchar_t* GetItemId()              const override { return L"MijiaPowerW"; }
    const wchar_t* GetItemLableText()       const override;
    const wchar_t* GetItemValueText()       const override;
    const wchar_t* GetItemValueSampleText() const override { return L"9999.9W"; }

private:
    class CMijiaPowerPlugin* m_plugin = nullptr;
    mutable std::wstring m_valueText;
    mutable std::wstring m_labelText;
};

// ─────────────────────────────────────────────
// 插件主类
// ─────────────────────────────────────────────
class CMijiaPowerPlugin : public ITMPlugin {
public:
    CMijiaPowerPlugin();
    ~CMijiaPowerPlugin();

    // ── ITMPlugin 接口实现 ──
    IPluginItem*   GetItem(int index)  override;
    int            GetItemCount() const { return 1; }
    void           DataRequired()      override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;

    // 返回值改为 OptionReturn（官方接口）
    OptionReturn   ShowOptionsDialog(void* hParent) override;

    const wchar_t* GetTooltipInfo()    override;

    // API v7: 主程序调用此函数完成初始化，传入 ITrafficMonitor*
    void           OnInitialize(ITrafficMonitor* pApp) override;

    // 配置目录通过 OnExtenedInfo(EI_CONFIG_DIR, ...) 传入
    void           OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

    // 安全关闭（DllMain DLL_PROCESS_DETACH 阶段调用，detach 线程避免死锁）
    void Shutdown() {
        m_stopFlag = true;
        if (m_sampleThread.joinable())
            m_sampleThread.detach();
    }

    // 供 CPowerItem 访问
    double         GetCurrentWatts()   const;
    bool           IsConnected()       const { return m_connected.load(); }
    const PowerHistory& GetHistory()   const { return m_history; }

private:
    ITrafficMonitor*   m_pTM = nullptr;
    CPowerItem         m_powerItem;
    PowerHistory       m_history;

    // 后台采集线程
    std::thread        m_sampleThread;
    std::atomic<bool>  m_stopFlag{ false };
    std::atomic<bool>  m_connected{ false };
    std::atomic<double> m_currentWatts{ 0.0 };

    // tooltip缓存
    mutable std::wstring m_tooltipText;

    bool               m_initialized = false;  // 防止重复初始化

    void StartSampling();  // 启动采样线程（在配置目录确定后调用）
    void SampleLoop();
    void PersistHistoryIfEnabled() const;
    void ConnectDevice();
    void DisconnectDevice();

    std::unique_ptr<MiioDevice> m_device;
    mutable std::mutex          m_deviceMutex;
};

// DLL 导出
extern "C" __declspec(dllexport)
ITMPlugin* TMPluginGetInstance();
