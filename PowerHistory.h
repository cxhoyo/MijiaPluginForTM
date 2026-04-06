// PowerHistory.h - 功率历史记录与统计管理
#pragma once
#include "pch.h"

struct PowerSample {
    double   timestamp; // Unix 时间戳（秒，浮点）
    double   watts;     // 功率（W）
};

struct PowerStats {
    double maxW   = 0.0;
    double minW   = 0.0;
    double avgW   = 0.0;
    double lastW  = 0.0;
    size_t count  = 0;
    bool   valid  = false;
};

class PowerHistory {
public:
    static const size_t MAX_REALTIME = 300;   // 最近5分钟（2秒一次=150，取300富裕）
    static const size_t MAX_LONG     = 10080; // 7天（1分钟一次）

    PowerHistory();
    ~PowerHistory() = default;

    // 添加一个实时采样点
    void AddSample(double watts);

    // 最近 N 秒的实时样本
    std::vector<PowerSample> GetRecentSamples(int seconds = 600) const;

    // 长期（按分钟聚合）样本
    std::vector<PowerSample> GetLongSamples(int hours = 1) const;

    // 当前瞬时功率（最后一次采样）
    double GetCurrentWatts() const;
    bool   HasData() const;

    // 统计
    PowerStats GetStats(int seconds) const;       // 实时段
    PowerStats GetLongStats(int hours) const;     // 长期段

    // 持久化（追加导出到按月拆分的 CSV 目录；启动时按流式读取恢复近期汇总）
    void SaveToFile(const std::wstring& filePath) const;
    void LoadFromFile(const std::wstring& filePath);

private:
    mutable std::mutex       m_mutex;
    std::deque<PowerSample>  m_realtime;  // 实时环形缓冲
    std::deque<PowerSample>  m_longterm;  // 长期（分钟级）
    mutable std::deque<PowerSample>  m_pendingPersist; // 等待追加到CSV的样本
    double                   m_lastMinTs = 0.0; // 最后长期采样的分钟时间戳

    static double Now();
    static std::wstring FormatLocalTimestamp(double timestamp);
    static bool TryParseLocalTimestamp(const std::wstring& text, double& timestamp);
    PowerStats CalcStats(const std::vector<PowerSample>& pts) const;
};
