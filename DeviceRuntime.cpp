// DeviceRuntime.cpp - independent connection and sampling state for one plug
#include "pch.h"
#include "DeviceRuntime.h"

DeviceRuntime::DeviceRuntime(const DeviceConfig& config) : m_config(config) {}

DeviceRuntime::~DeviceRuntime() {
    Stop(true);
}

void DeviceRuntime::Start() {
    if (m_thread.joinable()) return;
    m_stop = false;
    LoadHistory();
    m_thread = std::thread(&DeviceRuntime::SampleLoop, this);
}

void DeviceRuntime::Stop(bool waitForThread) {
    m_stop = true;
    if (m_thread.joinable()) {
        if (waitForThread) m_thread.join();
        else m_thread.detach();
    }
    if (waitForThread) SaveHistory();
}

void DeviceRuntime::UpdateConfig(const DeviceConfig& config) {
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = config;
    }
    m_pendingRemoval = false;
    ++m_configRevision;
}

void DeviceRuntime::SetSharedSettings(int updateIntervalSec, bool enableRecording) {
    m_updateIntervalSec = (std::max)(1, (std::min)(60, updateIntervalSec));
    m_enableRecording = enableRecording;
}

void DeviceRuntime::MarkPendingRemoval() {
    m_pendingRemoval = true;
    m_connected = false;
    ++m_configRevision;
}

DeviceConfig DeviceRuntime::GetConfig() const {
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

std::string DeviceRuntime::ToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        result.data(), length, nullptr, nullptr);
    return result;
}

void DeviceRuntime::LoadHistory() {
    if (!m_enableRecording.load()) return;
    const auto config = GetConfig();
    const auto path = ConfigManager::Instance().GetHistoryFilePath(config.itemId);
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        m_history.LoadFromFile(path);
        return;
    }

    if (config.itemId == L"MijiaPowerW") {
        const auto legacyPath = ConfigManager::Instance().GetLegacyHistoryFilePath();
        if (GetFileAttributesW(legacyPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            m_history.LoadFromFile(legacyPath);
            m_history.SaveToFile(path);
        }
    }
}

void DeviceRuntime::SaveHistory() const {
    if (!m_enableRecording.load()) return;
    const auto config = GetConfig();
    m_history.SaveToFile(ConfigManager::Instance().GetHistoryFilePath(config.itemId));
}

void DeviceRuntime::SampleLoop() {
    std::unique_ptr<MiioDevice> device;
    unsigned long long activeRevision = 0;
    int elapsedMilliseconds = 1000000;

    while (!m_stop.load()) {
        Sleep(100);
        if (m_pendingRemoval.load()) {
            device.reset();
            m_connected = false;
            continue;
        }

        const auto revision = m_configRevision.load();
        if (revision != activeRevision) {
            activeRevision = revision;
            device.reset();
            m_connected = false;
            elapsedMilliseconds = 1000000;
        }

        elapsedMilliseconds += 100;
        const int intervalMilliseconds = m_updateIntervalSec.load() * 1000;
        if (elapsedMilliseconds < intervalMilliseconds) continue;
        elapsedMilliseconds = 0;

        const DeviceConfig config = GetConfig();
        if (config.deviceIp.empty() || config.deviceToken.empty()) {
            device.reset();
            m_connected = false;
            continue;
        }

        try {
            if (!device) {
                device = std::make_unique<MiioDevice>(ToUtf8(config.deviceIp),
                                                       ToUtf8(config.deviceToken), 5000);
            }

            double watts = 0.0;
            if (device->GetPower(watts)) {
                m_currentWatts = watts;
                m_connected = true;
                if (m_enableRecording.load()) m_history.AddSample(watts);
            } else {
                device.reset();
                m_connected = false;
            }
        } catch (...) {
            device.reset();
            m_connected = false;
        }
    }

    SaveHistory();
}
