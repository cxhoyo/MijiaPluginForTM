// PluginConfig.h - multi-device plugin configuration
#pragma once
#include "pch.h"

struct DeviceConfig {
    std::wstring itemId;
    std::wstring deviceIp;
    std::wstring deviceToken;
    std::wstring deviceName = L"米家插座";
};

struct PluginConfig {
    std::vector<DeviceConfig> devices;

    bool enableRecording = true;
    bool showLabel = true;
    bool showUnit = true;
    int updateIntervalSec = 3;
    int decimalPlaces = 1;
};

class ConfigManager {
public:
    static ConfigManager& Instance() {
        static ConfigManager inst;
        return inst;
    }

    void SetConfigDir(const std::wstring& dir) { m_dir = dir; }
    bool Load();
    bool Save() const;

    PluginConfig& Get() { return m_cfg; }
    const PluginConfig& Get() const { return m_cfg; }

    std::wstring CreateItemId() const;
    std::wstring GetHistoryFilePath(const std::wstring& itemId) const;
    std::wstring GetLegacyHistoryFilePath() const;
    std::wstring GetIniPath() const;

private:
    static constexpr int CONFIG_VERSION = 2;

    std::wstring m_dir;
    PluginConfig m_cfg;

    bool LoadVersion2(const std::wstring& path);
    bool MigrateLegacy(const std::wstring& path);
    std::vector<DeviceConfig> ParseLegacyDevices(const std::wstring& path) const;

    static std::wstring ReadIniString(const std::wstring& section, const std::wstring& key,
                                      const std::wstring& def, const std::wstring& path);
    static int ReadIniInt(const std::wstring& section, const std::wstring& key,
                          int def, const std::wstring& path);
    static bool ReadIniBool(const std::wstring& section, const std::wstring& key,
                            bool def, const std::wstring& path);
    static std::wstring DecodeTextFile(const std::wstring& path);
    static std::wstring Trim(std::wstring value);
    static bool EqualsIgnoreCase(const std::wstring& lhs, const std::wstring& rhs);
};
