// MijiaPowerPlugin.h - multi-device TrafficMonitor plugin
#pragma once
#include "pch.h"
#include "PluginInterface.h"
#include "DeviceRuntime.h"

class CMijiaPowerPlugin;

class CPowerItem : public IPluginItem {
public:
    CPowerItem(CMijiaPowerPlugin* plugin, std::shared_ptr<DeviceRuntime> runtime);

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override { return true; }
    int GetItemWidth() const override;
    int GetItemWidthEx(void* hDC) const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool darkMode) override;

    std::shared_ptr<DeviceRuntime> Runtime() const { return m_runtime; }

private:
    CMijiaPowerPlugin* m_plugin;
    std::shared_ptr<DeviceRuntime> m_runtime;
    mutable std::wstring m_itemName;
    mutable std::wstring m_itemId;
    mutable std::wstring m_labelText;
    mutable std::wstring m_valueText;
    mutable std::wstring m_sampleText;

    std::wstring FormatLabel() const;
    std::wstring FormatValue(bool sample) const;
};

class CMijiaPowerPlugin : public ITMPlugin {
public:
    CMijiaPowerPlugin() = default;
    ~CMijiaPowerPlugin();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetTooltipInfo() override;
    void OnInitialize(ITrafficMonitor* pApp) override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;

    void Shutdown();
    COLORREF LabelColor() const { return m_labelColor.load(); }
    COLORREF ValueColor() const { return m_valueColor.load(); }
    bool IsDrawingTaskbar() const { return m_drawingTaskbar.load(); }

private:
    ITrafficMonitor* m_pTM = nullptr;
    std::vector<std::shared_ptr<DeviceRuntime>> m_runtimes;
    std::vector<std::unique_ptr<CPowerItem>> m_items;
    std::atomic<COLORREF> m_labelColor{ RGB(255, 255, 255) };
    std::atomic<COLORREF> m_valueColor{ RGB(255, 255, 255) };
    std::atomic<bool> m_drawingTaskbar{ false };
    mutable std::wstring m_tooltipText;
    bool m_initialized = false;

    void Initialize(const std::wstring& configDir);
    void StartRuntimes();
    void ApplyConfigurationToRuntimes();
};

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance();
