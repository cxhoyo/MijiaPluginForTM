// PowerHistory.cpp - 功率历史记录实现
#include "pch.h"
#include "PowerHistory.h"
#include <chrono>
#include <filesystem>
#include <sstream>
#include <fstream>

double PowerHistory::Now() {
    auto tp = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(tp).count();
}

PowerHistory::PowerHistory() = default;

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

// ─── 持久化（极简 JSON，无第三方库）────
void PowerHistory::SaveToFile(const std::wstring& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (filePath.empty()) return;

    std::filesystem::path path(filePath);
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::wofstream f(filePath);
    if (!f.is_open()) return;
    f << L"[";
    bool first = true;
    for (const auto& s : m_longterm) {
        if (!first) f << L",";
        f << L"{\"t\":" << std::fixed << std::setprecision(1) << s.timestamp
          << L",\"w\":" << std::setprecision(2) << s.watts << L"}";
        first = false;
    }
    f << L"]";
}

void PowerHistory::LoadFromFile(const std::wstring& filePath) {
    std::wifstream f(filePath);
    if (!f.is_open()) return;
    std::wstring content((std::istreambuf_iterator<wchar_t>(f)), std::istreambuf_iterator<wchar_t>());

    std::lock_guard<std::mutex> lock(m_mutex);
    m_longterm.clear();

    // 简单解析 JSON 数组
    size_t pos = 0;
    while (pos < content.size()) {
        auto ts = content.find(L"\"t\":", pos);
        auto ws = content.find(L"\"w\":", pos);
        if (ts == std::wstring::npos || ws == std::wstring::npos) break;
        try {
            double t = std::stod(content.substr(ts+4));
            double w = std::stod(content.substr(ws+4));
            m_longterm.push_back({ t, w });
            if (m_longterm.size() > MAX_LONG) m_longterm.pop_front();
        } catch (...) {}
        pos = std::max(ts, ws) + 4;
    }
    if (!m_longterm.empty())
        m_lastMinTs = std::floor(m_longterm.back().timestamp / 60.0);
}
