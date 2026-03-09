#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace limitless {

using Clock     = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using LogEntry  = std::pair<TimePoint, int>;  // (timestamp, cost)

// ---------------------------------------------------------------------------
// Abstract store
// ---------------------------------------------------------------------------

class BaseStore {
public:
    virtual ~BaseStore() = default;

    /// Return all log entries within the last `window` seconds, pruning older ones.
    virtual std::vector<LogEntry> get_log(const std::string& key,
                                          double window_seconds) = 0;

    /// Append a new entry and prune entries older than `window_seconds`.
    virtual void append(const std::string& key,
                        TimePoint timestamp,
                        int cost,
                        double window_seconds) = 0;
};

// ---------------------------------------------------------------------------
// In-memory store  (thread-safe via std::mutex)
// ---------------------------------------------------------------------------

class MemoryStore : public BaseStore {
public:
    std::vector<LogEntry> get_log(const std::string& key,
                                  double window_seconds) override;

    void append(const std::string& key,
                TimePoint timestamp,
                int cost,
                double window_seconds) override;

private:
    // Each key maps to a deque of (timestamp, cost) sorted ascending
    std::unordered_map<std::string, std::deque<LogEntry>> logs_;
    std::mutex mutex_;
};

} // namespace limitless