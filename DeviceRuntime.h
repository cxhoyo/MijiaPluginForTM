// DeviceRuntime.h - independent connection and sampling state for one plug
#pragma once
#include "pch.h"
#include "MiioDevice.h"
#include "PluginConfig.h"
#include "PowerHistory.h"

class DeviceRuntime {
public:
    explicit DeviceRuntime(const DeviceConfig& config);
    ~DeviceRuntime();

    DeviceRuntime(const DeviceRuntime&) = delete;
    DeviceRuntime& operator=(const DeviceRuntime&) = delete;

    void Start();
    void Stop(bool waitForThread = true);
    void UpdateConfig(const DeviceConfig& config);
    void SetSharedSettings(int updateIntervalSec, bool enableRecording);
    void MarkPendingRemoval();

    DeviceConfig GetConfig() const;
    double GetCurrentWatts() const { return m_currentWatts.load(); }
    bool IsConnected() const { return m_connected.load(); }
    bool IsPendingRemoval() const { return m_pendingRemoval.load(); }
    const PowerHistory& GetHistory() const { return m_history; }

private:
    mutable std::mutex m_configMutex;
    DeviceConfig m_config;
    std::atomic<unsigned long long> m_configRevision{ 1 };

    std::thread m_thread;
    std::atomic<bool> m_stop{ false };
    std::atomic<bool> m_connected{ false };
    std::atomic<bool> m_pendingRemoval{ false };
    std::atomic<double> m_currentWatts{ 0.0 };
    std::atomic<int> m_updateIntervalSec{ 3 };
    std::atomic<bool> m_enableRecording{ true };
    PowerHistory m_history;

    void SampleLoop();
    void LoadHistory();
    void SaveHistory() const;
    static std::string ToUtf8(const std::wstring& value);
};
