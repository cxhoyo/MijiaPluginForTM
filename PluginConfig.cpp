// PluginConfig.cpp - multi-device INI storage and legacy migration
#include "pch.h"
#include "PluginConfig.h"
#include <cwctype>

namespace {
std::wstring DeviceSection(size_t index) {
    return L"Device" + std::to_wstring(index);
}

bool WriteIniString(const std::wstring& section, const std::wstring& key,
                    const std::wstring& value, const std::wstring& path) {
    return WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), path.c_str()) != FALSE;
}

bool WriteIniInt(const std::wstring& section, const std::wstring& key,
                 int value, const std::wstring& path) {
    return WriteIniString(section, key, std::to_wstring(value), path);
}
}

std::wstring ConfigManager::GetIniPath() const {
    return m_dir + L"\\MijiaPower.ini";
}

std::wstring ConfigManager::GetHistoryFilePath(const std::wstring& itemId) const {
    return m_dir + L"\\MijiaPower_history_" + itemId + L".json";
}

std::wstring ConfigManager::GetLegacyHistoryFilePath() const {
    return m_dir + L"\\MijiaPower_history.json";
}

std::wstring ConfigManager::ReadIniString(const std::wstring& section, const std::wstring& key,
                                           const std::wstring& def, const std::wstring& path) {
    wchar_t buf[1024] = {};
    GetPrivateProfileStringW(section.c_str(), key.c_str(), def.c_str(), buf,
                             static_cast<DWORD>(_countof(buf)), path.c_str());
    return buf;
}

int ConfigManager::ReadIniInt(const std::wstring& section, const std::wstring& key,
                              int def, const std::wstring& path) {
    return static_cast<int>(GetPrivateProfileIntW(section.c_str(), key.c_str(), def, path.c_str()));
}

bool ConfigManager::ReadIniBool(const std::wstring& section, const std::wstring& key,
                                bool def, const std::wstring& path) {
    return ReadIniInt(section, key, def ? 1 : 0, path) != 0;
}

std::wstring ConfigManager::Trim(std::wstring value) {
    auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](wchar_t ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

bool ConfigManager::EqualsIgnoreCase(const std::wstring& lhs, const std::wstring& rhs) {
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin(),
        [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
}

std::wstring ConfigManager::DecodeTextFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return {};
    }

    std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD bytesRead = 0;
    bool ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &bytesRead, nullptr) != FALSE;
    CloseHandle(file);
    if (!ok) return {};
    bytes.resize(bytesRead);

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t characterCount = (bytes.size() - 2) / sizeof(wchar_t);
        std::wstring result(characterCount, L'\0');
        memcpy(result.data(), bytes.data() + 2, characterCount * sizeof(wchar_t));
        return result;
    }

    size_t offset = 0;
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        offset = 3;
    }

    int sourceLength = static_cast<int>(bytes.size() - offset);
    int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        bytes.data() + offset, sourceLength, nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (wideLength == 0) {
        codePage = 936; // Legacy releases were primarily used on Simplified Chinese Windows.
        flags = MB_ERR_INVALID_CHARS;
        wideLength = MultiByteToWideChar(codePage, flags, bytes.data(),
            static_cast<int>(bytes.size()), nullptr, 0);
        if (wideLength == 0) {
            codePage = CP_ACP;
            flags = 0;
            wideLength = MultiByteToWideChar(codePage, flags, bytes.data(),
                static_cast<int>(bytes.size()), nullptr, 0);
        }
        offset = 0;
        sourceLength = static_cast<int>(bytes.size());
    }
    if (wideLength <= 0) return {};

    std::wstring result(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(codePage, flags, bytes.data() + offset, sourceLength,
                        result.data(), wideLength);
    return result;
}

std::vector<DeviceConfig> ConfigManager::ParseLegacyDevices(const std::wstring& path) const {
    std::vector<DeviceConfig> devices;
    std::wistringstream input(DecodeTextFile(path));
    std::wstring line;
    bool inDevice = false;
    DeviceConfig current;
    current.deviceName.clear();

    auto finishDevice = [&]() {
        if (inDevice && (!current.deviceIp.empty() || !current.deviceToken.empty() || !current.deviceName.empty())) {
            if (current.deviceName.empty()) current.deviceName = L"米家插座";
            devices.push_back(current);
        }
        current = DeviceConfig{};
        current.deviceName.clear();
    };

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == L';' || trimmed[0] == L'#') continue;

        if (trimmed.front() == L'[' && trimmed.back() == L']') {
            finishDevice();
            std::wstring section = Trim(trimmed.substr(1, trimmed.size() - 2));
            inDevice = EqualsIgnoreCase(section, L"Device");
            continue;
        }
        if (!inDevice) continue;

        size_t separator = trimmed.find(L'=');
        if (separator == std::wstring::npos) continue;
        std::wstring key = Trim(trimmed.substr(0, separator));
        std::wstring value = Trim(trimmed.substr(separator + 1));
        if (EqualsIgnoreCase(key, L"IP")) current.deviceIp = value;
        else if (EqualsIgnoreCase(key, L"Token")) current.deviceToken = value;
        else if (EqualsIgnoreCase(key, L"Name")) current.deviceName = value;
    }
    finishDevice();
    return devices;
}

bool ConfigManager::LoadVersion2(const std::wstring& path) {
    int count = ReadIniInt(L"Plugin", L"DeviceCount", 0, path);
    if (count < 0) count = 0;
    if (count > 128) count = 128;

    m_cfg.devices.clear();
    for (int i = 0; i < count; ++i) {
        std::wstring section = DeviceSection(static_cast<size_t>(i));
        DeviceConfig device;
        device.itemId = ReadIniString(section, L"ItemId", L"", path);
        device.deviceIp = ReadIniString(section, L"IP", L"", path);
        device.deviceToken = ReadIniString(section, L"Token", L"", path);
        device.deviceName = ReadIniString(section, L"Name", L"米家插座", path);
        const bool validId = !device.itemId.empty() && std::all_of(device.itemId.begin(), device.itemId.end(),
            [](wchar_t ch) { return (ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') ||
                                    (ch >= L'a' && ch <= L'z'); });
        const bool duplicateId = std::any_of(m_cfg.devices.begin(), m_cfg.devices.end(),
            [&](const DeviceConfig& existing) { return existing.itemId == device.itemId; });
        if (!validId || duplicateId) device.itemId = CreateItemId();
        m_cfg.devices.push_back(std::move(device));
    }
    return true;
}

bool ConfigManager::MigrateLegacy(const std::wstring& path) {
    auto devices = ParseLegacyDevices(path);
    if (devices.empty()) return false;

    for (size_t i = 0; i < devices.size(); ++i) {
        devices[i].itemId = (i == 0) ? L"MijiaPowerW" : CreateItemId();
    }
    m_cfg.devices = std::move(devices);

    std::wstring backup = path + L".legacy.bak";
    if (!CopyFileW(path.c_str(), backup.c_str(), TRUE) && GetLastError() != ERROR_FILE_EXISTS) {
        return false;
    }
    return Save();
}

bool ConfigManager::Load() {
    const std::wstring path = GetIniPath();

    m_cfg.enableRecording = ReadIniBool(L"Plugin", L"EnableRecording", true, path);
    m_cfg.showLabel = ReadIniBool(L"Plugin", L"ShowLabel", true, path);
    m_cfg.showUnit = ReadIniBool(L"Plugin", L"ShowUnit", true, path);
    m_cfg.updateIntervalSec = ReadIniInt(L"Plugin", L"UpdateIntervalSec", 3, path);
    m_cfg.decimalPlaces = ReadIniInt(L"Plugin", L"DecimalPlaces", 1, path);

    m_cfg.updateIntervalSec = (std::max)(1, (std::min)(60, m_cfg.updateIntervalSec));
    m_cfg.decimalPlaces = (std::max)(0, (std::min)(2, m_cfg.decimalPlaces));

    int version = ReadIniInt(L"Plugin", L"ConfigVersion", 0, path);
    if (version >= CONFIG_VERSION) return LoadVersion2(path);
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return MigrateLegacy(path);
    m_cfg.devices.clear();
    return true;
}

bool ConfigManager::Save() const {
    const std::wstring path = GetIniPath();
    const std::wstring tempPath = path + L".tmp";
    DeleteFileW(tempPath.c_str());

    HANDLE tempFile = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (tempFile == INVALID_HANDLE_VALUE) return false;
    const wchar_t bom = 0xFEFF;
    DWORD bytesWritten = 0;
    const bool bomWritten = WriteFile(tempFile, &bom, sizeof(bom), &bytesWritten, nullptr) != FALSE;
    CloseHandle(tempFile);
    if (!bomWritten || bytesWritten != sizeof(bom)) {
        DeleteFileW(tempPath.c_str());
        return false;
    }

    bool ok = true;
    ok &= WriteIniInt(L"Plugin", L"ConfigVersion", CONFIG_VERSION, tempPath);
    ok &= WriteIniInt(L"Plugin", L"DeviceCount", static_cast<int>(m_cfg.devices.size()), tempPath);
    ok &= WriteIniInt(L"Plugin", L"EnableRecording", m_cfg.enableRecording ? 1 : 0, tempPath);
    ok &= WriteIniInt(L"Plugin", L"ShowLabel", m_cfg.showLabel ? 1 : 0, tempPath);
    ok &= WriteIniInt(L"Plugin", L"ShowUnit", m_cfg.showUnit ? 1 : 0, tempPath);
    ok &= WriteIniInt(L"Plugin", L"UpdateIntervalSec", m_cfg.updateIntervalSec, tempPath);
    ok &= WriteIniInt(L"Plugin", L"DecimalPlaces", m_cfg.decimalPlaces, tempPath);

    for (size_t i = 0; i < m_cfg.devices.size(); ++i) {
        const auto& device = m_cfg.devices[i];
        const std::wstring section = DeviceSection(i);
        ok &= WriteIniString(section, L"ItemId", device.itemId, tempPath);
        ok &= WriteIniString(section, L"IP", device.deviceIp, tempPath);
        ok &= WriteIniString(section, L"Token", device.deviceToken, tempPath);
        ok &= WriteIniString(section, L"Name", device.deviceName, tempPath);
    }

    WritePrivateProfileStringW(nullptr, nullptr, nullptr, tempPath.c_str());
    if (!ok || !MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.c_str());
        return false;
    }
    return true;
}

std::wstring ConfigManager::CreateItemId() const {
    static std::atomic<unsigned long long> counter{ 0 };
    LARGE_INTEGER ticks{};
    QueryPerformanceCounter(&ticks);
    unsigned long long value = static_cast<unsigned long long>(ticks.QuadPart) ^
        (GetTickCount64() << 17) ^ (++counter << 33) ^ GetCurrentProcessId();

    std::wostringstream id;
    id << L"MijiaPower" << std::hex << std::uppercase << std::setw(16)
       << std::setfill(L'0') << value;
    std::wstring result = id.str();
    while (std::any_of(m_cfg.devices.begin(), m_cfg.devices.end(),
        [&](const DeviceConfig& device) { return device.itemId == result; })) {
        result += L"A";
    }
    return result;
}
