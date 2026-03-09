#include "limitless/store.hpp"

#include <algorithm>
#include <unordered_map>

namespace limitless {

// ---------------------------------------------------------------------------
// MemoryStore
// ---------------------------------------------------------------------------

std::vector<LogEntry> MemoryStore::get_log(const std::string& key,
                                            double window_seconds)
{
    auto cutoff = Clock::now() -
                  std::chrono::duration_cast<Clock::duration>(
                      std::chrono::duration<double>(window_seconds));

    std::lock_guard<std::mutex> lock(mutex_);

    auto& deq = logs_[key];

    // Prune expired entries from the front (deque is sorted ascending)
    while (!deq.empty() && deq.front().first <= cutoff) {
        deq.pop_front();
    }

    return std::vector<LogEntry>(deq.begin(), deq.end());
}

void MemoryStore::append(const std::string& key,
                          TimePoint timestamp,
                          int cost,
                          double window_seconds)
{
    auto cutoff = timestamp -
                  std::chrono::duration_cast<Clock::duration>(
                      std::chrono::duration<double>(window_seconds));

    std::lock_guard<std::mutex> lock(mutex_);

    auto& deq = logs_[key];
    deq.push_back({timestamp, cost});

    // Prune from the front
    while (!deq.empty() && deq.front().first <= cutoff) {
        deq.pop_front();
    }
}

} // namespace limitless