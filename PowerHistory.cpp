// PowerHistory.cpp - 功率历史记录实现
#include "pch.h"
#include "PowerHistory.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <fstream>

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

    std::filesystem::path path(filePath);
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::error_code ec;
    bool writeHeader = !std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) == 0;

    std::wofstream f(filePath, std::ios::app);
    if (!f.is_open()) return;

    if (writeHeader) {
        f << L"本地时间,功率(W)\r\n";
    }

    for (const auto& s : m_pendingPersist) {
        f << FormatLocalTimestamp(s.timestamp)
          << L"," << std::fixed << std::setprecision(2) << s.watts
          << L"\r\n";
    }

    if (!f.fail()) {
        m_pendingPersist.clear();
    }
}

void PowerHistory::LoadFromFile(const std::wstring& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    (void)filePath;
    // CSV 历史文件仅用于追加导出，不在启动时回读。
    m_pendingPersist.clear();
}
