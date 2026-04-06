// PowerHistory.cpp - 功率历史记录实现
#include "pch.h"
#include "PowerHistory.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace {
constexpr wchar_t kHistoryCsvHeader[] = L"本地时间,功率(W)";
constexpr unsigned char kUtf8Bom[] = { 0xEF, 0xBB, 0xBF };

struct LocalMonthKey {
    int year = 0;
    int month = 0;
};

LocalMonthKey GetLocalMonthKey(double timestamp) {
    std::time_t timeValue = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);
    return { localTime.tm_year + 1900, localTime.tm_mon + 1 };
}

LocalMonthKey NextMonth(LocalMonthKey value) {
    ++value.month;
    if (value.month > 12) {
        value.month = 1;
        ++value.year;
    }
    return value;
}

bool IsSameMonth(LocalMonthKey lhs, LocalMonthKey rhs) {
    return lhs.year == rhs.year && lhs.month == rhs.month;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

const std::string& GetHistoryCsvHeaderUtf8() {
    static const std::string header = WideToUtf8(kHistoryCsvHeader);
    return header;
}

std::filesystem::path BuildMonthlyHistoryPath(const std::filesystem::path& historyDirectory, LocalMonthKey month) {
    std::wostringstream name;
    name << std::setw(4) << std::setfill(L'0') << month.year
         << L"-" << std::setw(2) << std::setfill(L'0') << month.month
         << L".csv";
    return historyDirectory / name.str();
}

std::vector<std::filesystem::path> CollectRecoveryHistoryPaths(const std::wstring& historyDirectory, double earliestTimestamp, double latestTimestamp) {
    std::vector<std::filesystem::path> paths;
    if (historyDirectory.empty()) {
        return paths;
    }

    const std::filesystem::path directoryPath(historyDirectory);
    std::error_code ec;
    LocalMonthKey month = GetLocalMonthKey(earliestTimestamp);
    const LocalMonthKey endMonth = GetLocalMonthKey(latestTimestamp);
    while (true) {
        auto monthlyPath = BuildMonthlyHistoryPath(directoryPath, month);
        if (std::filesystem::exists(monthlyPath, ec)) {
            paths.push_back(std::move(monthlyPath));
        }
        if (IsSameMonth(month, endMonth)) {
            break;
        }
        month = NextMonth(month);
    }

    return paths;
}

void AppendRecoveredSample(std::deque<PowerSample>& buffer, const PowerSample& sample, size_t maxSize) {
    const double minuteBucket = std::floor(sample.timestamp / 60.0);
    if (!buffer.empty() && std::floor(buffer.back().timestamp / 60.0) == minuteBucket) {
        buffer.back() = sample;
        return;
    }

    buffer.push_back(sample);
    while (buffer.size() > maxSize) {
        buffer.pop_front();
    }
}

bool TryParseCsvTimestamp(const std::wstring& text, double& timestamp) {
    std::tm localTime{};
    std::wistringstream iss(text);
    iss >> std::get_time(&localTime, L"%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return false;
    }

    std::time_t timeValue = std::mktime(&localTime);
    if (timeValue == static_cast<std::time_t>(-1)) {
        return false;
    }

    timestamp = static_cast<double>(timeValue);
    return true;
}

void LoadSamplesFromStream(std::istream& stream,
                           std::deque<PowerSample>& loadedRealtime,
                           std::deque<PowerSample>& loadedLongterm,
                           double realtimeCutoff,
                           double longCutoff,
                           double& lastMinTs) {
    std::string line;
    bool firstLine = true;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == kUtf8Bom[0] &&
                static_cast<unsigned char>(line[1]) == kUtf8Bom[1] &&
                static_cast<unsigned char>(line[2]) == kUtf8Bom[2]) {
                line.erase(0, 3);
            }

            if (line == GetHistoryCsvHeaderUtf8() || line == "LocalTime,Watts") {
                continue;
            }
        }

        size_t commaPos = line.find(',');
        if (commaPos == std::string::npos) {
            continue;
        }

        double timestamp = 0.0;
        if (!TryParseCsvTimestamp(Utf8ToWide(line.substr(0, commaPos)), timestamp)) {
            continue;
        }

        double watts = 0.0;
        try {
            watts = std::stod(line.substr(commaPos + 1));
        } catch (...) {
            continue;
        }

        PowerSample sample{ timestamp, watts };

        if (sample.timestamp >= realtimeCutoff) {
            AppendRecoveredSample(loadedRealtime, sample, PowerHistory::MAX_REALTIME);
        }

        if (sample.timestamp >= longCutoff) {
            AppendRecoveredSample(loadedLongterm, sample, PowerHistory::MAX_LONG);
            lastMinTs = std::floor(sample.timestamp / 60.0);
        }
    }
}
}

double PowerHistory::Now() {
    auto tp = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(tp).count();
}

PowerHistory::PowerHistory() = default;

std::wstring PowerHistory::FormatLocalTimestamp(double timestamp) {
    std::time_t timeValue = static_cast<std::time_t>(timestamp);
    std::tm localTime{};
    localtime_s(&localTime, &timeValue);

    wchar_t buffer[32]{};
    wcsftime(buffer, _countof(buffer), L"%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

bool PowerHistory::TryParseLocalTimestamp(const std::wstring& text, double& timestamp) {
    std::tm localTime{};
    std::wistringstream iss(text);
    iss >> std::get_time(&localTime, L"%Y-%m-%d %H:%M:%S");
    if (iss.fail()) {
        return false;
    }

    std::time_t timeValue = std::mktime(&localTime);
    if (timeValue == static_cast<std::time_t>(-1)) {
        return false;
    }

    timestamp = static_cast<double>(timeValue);
    return true;
}

void PowerHistory::AddSample(double watts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    double ts = Now();

    PowerSample s{ ts, watts };
    m_realtime.push_back(s);
    while (m_realtime.size() > MAX_REALTIME)
        m_realtime.pop_front();

    // 按分钟聚合长期记录
    double curMin = std::floor(ts / 60.0);
    if (curMin > m_lastMinTs) {
        m_lastMinTs = curMin;
        m_longterm.push_back(s);
        m_pendingPersist.push_back(s);
        while (m_longterm.size() > MAX_LONG)
            m_longterm.pop_front();
    }
}

double PowerHistory::GetCurrentWatts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_realtime.empty()) return 0.0;
    return m_realtime.back().watts;
}

bool PowerHistory::HasData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_realtime.empty();
}

std::vector<PowerSample> PowerHistory::GetRecentSamples(int seconds) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double cutoff = Now() - seconds;
    std::vector<PowerSample> result;
    for (const auto& s : m_realtime)
        if (s.timestamp >= cutoff) result.push_back(s);
    return result;
}

std::vector<PowerSample> PowerHistory::GetLongSamples(int hours) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double cutoff = Now() - hours * 3600.0;
    std::vector<PowerSample> result;
    for (const auto& s : m_longterm)
        if (s.timestamp >= cutoff) result.push_back(s);
    return result;
}

PowerStats PowerHistory::CalcStats(const std::vector<PowerSample>& pts) const {
    PowerStats st;
    if (pts.empty()) return st;
    st.valid = true;
    st.count = pts.size();
    st.maxW  = pts[0].watts;
    st.minW  = pts[0].watts;
    double sum = 0;
    for (const auto& p : pts) {
        if (p.watts > st.maxW) st.maxW = p.watts;
        if (p.watts < st.minW) st.minW = p.watts;
        sum += p.watts;
    }
    st.avgW  = sum / pts.size();
    st.lastW = pts.back().watts;
    return st;
}

PowerStats PowerHistory::GetStats(int seconds) const {
    return CalcStats(GetRecentSamples(seconds));
}

PowerStats PowerHistory::GetLongStats(int hours) const {
    return CalcStats(GetLongSamples(hours));
}

// ─── 持久化（追加 CSV，无第三方库）────
void PowerHistory::SaveToFile(const std::wstring& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (filePath.empty()) return;
    if (m_pendingPersist.empty()) return;

    std::filesystem::path historyDirectory(filePath);
    std::error_code ec;
    std::filesystem::create_directories(historyDirectory, ec);

    bool allSucceeded = true;
    std::filesystem::path currentPath;
    std::ofstream file;

    auto openMonthlyFile = [&](const PowerSample& sample) -> bool {
        std::filesystem::path monthlyPath = BuildMonthlyHistoryPath(historyDirectory, GetLocalMonthKey(sample.timestamp));
        if (monthlyPath == currentPath && file.is_open()) {
            return true;
        }

        if (file.is_open()) {
            file.close();
        }

        std::error_code ec;
        bool writeHeader = !std::filesystem::exists(monthlyPath, ec) || std::filesystem::file_size(monthlyPath, ec) == 0;

        file.open(monthlyPath, std::ios::app | std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        currentPath = std::move(monthlyPath);
        if (writeHeader) {
            file.write(reinterpret_cast<const char*>(kUtf8Bom), sizeof(kUtf8Bom));
            const std::string& header = GetHistoryCsvHeaderUtf8();
            file.write(header.data(), static_cast<std::streamsize>(header.size()));
            file.write("\r\n", 2);
            if (file.fail()) {
                file.close();
                return false;
            }
        }
        return true;
    };

    for (const auto& sample : m_pendingPersist) {
        if (!openMonthlyFile(sample)) {
            allSucceeded = false;
            break;
        }

        std::ostringstream line;
        line << WideToUtf8(FormatLocalTimestamp(sample.timestamp))
             << "," << std::fixed << std::setprecision(2) << sample.watts
             << "\r\n";
        const std::string text = line.str();
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (file.fail()) {
            allSucceeded = false;
            break;
        }
    }

    if (file.is_open()) {
        file.flush();
    }

    if (allSucceeded) {
        m_pendingPersist.clear();
    }
}

void PowerHistory::LoadFromFile(const std::wstring& filePath) {
    if (filePath.empty()) {
        return;
    }

    const double now = Now();
    const double realtimeCutoff = now - 600.0;          // tooltip 最近10分钟
    const double longCutoff = now - 7 * 24 * 3600.0;    // 保留最近7天分钟级样本

    std::deque<PowerSample> loadedRealtime;
    std::deque<PowerSample> loadedLongterm;
    double lastMinTs = 0.0;
    auto recoveryPaths = CollectRecoveryHistoryPaths(filePath, longCutoff, now);
    bool openedAnyFile = false;
    for (const auto& historyPath : recoveryPaths) {
        std::ifstream stream(historyPath, std::ios::binary);
        if (!stream.is_open()) {
            continue;
        }
        openedAnyFile = true;
        LoadSamplesFromStream(stream, loadedRealtime, loadedLongterm, realtimeCutoff, longCutoff, lastMinTs);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_realtime = openedAnyFile ? std::move(loadedRealtime) : std::deque<PowerSample>{};
    m_longterm = openedAnyFile ? std::move(loadedLongterm) : std::deque<PowerSample>{};
    m_pendingPersist.clear();
    m_lastMinTs = lastMinTs;
}
