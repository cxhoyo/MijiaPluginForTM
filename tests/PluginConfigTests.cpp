#include "pch.h"
#include "PluginConfig.h"

namespace {
void Require(bool condition, const wchar_t* message) {
    if (!condition) {
        fwprintf(stderr, L"FAILED: %ls\n", message);
        ExitProcess(1);
    }
}

void WriteLegacyGbkFile(const std::wstring& path, const std::wstring& content) {
    constexpr UINT legacyCodePage = 936;
    int length = WideCharToMultiByte(legacyCodePage, 0, content.c_str(), static_cast<int>(content.size()),
                                     nullptr, 0, nullptr, nullptr);
    Require(length > 0, L"encode fixture");
    std::string bytes(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(legacyCodePage, 0, content.c_str(), static_cast<int>(content.size()),
                        bytes.data(), length, nullptr, nullptr);

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, L"create fixture");
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
    CloseHandle(file);
    Require(ok && written == bytes.size(), L"write fixture");
}
}

int wmain() {
    wchar_t tempRoot[MAX_PATH] = {};
    Require(GetTempPathW(MAX_PATH, tempRoot) > 0, L"get temp directory");
    const std::wstring directory = std::wstring(tempRoot) + L"MijiaPowerConfigTest_" +
        std::to_wstring(GetCurrentProcessId());
    Require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE ||
            GetLastError() == ERROR_ALREADY_EXISTS, L"create test directory");

    const std::wstring iniPath = directory + L"\\MijiaPower.ini";
    const std::wstring legacy =
        L"[Device]\r\n"
        L"IP=192.0.2.10\r\n"
        L"Token=00112233445566778899aabbccddeeff\r\n"
        L"Name=电脑插座\r\n"
        L"[Plugin]\r\n"
        L"ShowLabel=1\r\n"
        L"ShowUnit=1\r\n"
        L"UpdateIntervalSec=3\r\n"
        L"DecimalPlaces=1\r\n"
        L"[Device]\r\n"
        L"IP=192.0.2.11\r\n"
        L"Token=ffeeddccbbaa99887766554433221100\r\n"
        L"Name=空调插座\r\n";
    WriteLegacyGbkFile(iniPath, legacy);

    auto& manager = ConfigManager::Instance();
    manager.SetConfigDir(directory);
    Require(manager.Load(), L"migrate legacy configuration");
    const auto firstLoad = manager.Get();
    Require(firstLoad.devices.size() == 2, L"import both repeated device sections");
    Require(firstLoad.devices[0].itemId == L"MijiaPowerW", L"preserve legacy first item id");
    Require(firstLoad.devices[0].deviceName == L"电脑插座", L"decode first ANSI device name");
    Require(firstLoad.devices[1].deviceName == L"空调插座", L"decode second ANSI device name");
    Require(firstLoad.devices[1].itemId != firstLoad.devices[0].itemId, L"assign unique second item id");
    Require(GetFileAttributesW((iniPath + L".legacy.bak").c_str()) != INVALID_FILE_ATTRIBUTES,
            L"create migration backup");
    Require(GetPrivateProfileIntW(L"Plugin", L"ConfigVersion", 0, iniPath.c_str()) == 2,
            L"write version 2 configuration");
    {
        HANDLE file = CreateFileW(iniPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(file != INVALID_HANDLE_VALUE, L"open migrated configuration");
        wchar_t bom = 0;
        DWORD bytesRead = 0;
        const bool read = ReadFile(file, &bom, sizeof(bom), &bytesRead, nullptr) != FALSE;
        CloseHandle(file);
        Require(read && bytesRead == sizeof(bom) && bom == 0xFEFF,
                L"write migrated configuration as Unicode INI");
    }

    const std::wstring secondId = firstLoad.devices[1].itemId;
    Require(manager.Load(), L"reload version 2 configuration");
    Require(manager.Get().devices.size() == 2, L"reload both devices");
    Require(manager.Get().devices[1].itemId == secondId, L"keep stable item id on reload");

    DeleteFileW((iniPath + L".legacy.bak").c_str());
    DeleteFileW(iniPath.c_str());
    RemoveDirectoryW(directory.c_str());
    wprintf(L"PluginConfig migration tests passed.\n");
    return 0;
}
