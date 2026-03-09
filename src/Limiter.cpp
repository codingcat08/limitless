#include "limitless/limiter.hpp"

#include <algorithm>
#include <numeric>
#include <thread>

namespace limitless {

// ---------------------------------------------------------------------------
// SlidingWindowLimiter
// ---------------------------------------------------------------------------

SlidingWindowLimiter::SlidingWindowLimiter(int                        limit,
                                            std::shared_ptr<BaseStore> store,
                                            std::string                key,
                                            double                     window_seconds)
    : limit_(limit), store_(std::move(store)), key_(std::move(key)), window_(window_seconds)
{}

void SlidingWindowLimiter::acquire(int cost) {
    while (true) {
        auto now = Clock::now();
        auto log = store_->get_log(key_, window_);

        // Sum cost of all entries currently in the window
        int used = std::accumulate(log.begin(), log.end(), 0,
            [](int acc, const LogEntry& e) { return acc + e.second; });

        if (used + cost <= limit_) {
            // Budget available — record and return
            store_->append(key_, now, cost, window_);
            return;
        }

        // Over budget — sleep until the oldest entry expires, freeing its cost
        auto oldest_ts = std::min_element(log.begin(), log.end(),
            [](const LogEntry& a, const LogEntry& b) {
                return a.first < b.first;
            })->first;

        auto window_dur = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(window_));

        auto wake_at   = oldest_ts + window_dur;
        auto sleep_for = wake_at - Clock::now();

        if (sleep_for > Clock::duration::zero()) {
            std::this_thread::sleep_for(sleep_for);
        }
        // Re-check after waking (another thread may have taken the slot)
    }
}

// ---------------------------------------------------------------------------
// LimitGuard  (RAII — blocks on construction)
// ---------------------------------------------------------------------------

LimitGuard::LimitGuard(SlidingWindowLimiter& req_bucket,
                        SlidingWindowLimiter& tok_bucket,
                        int                   token_cost)
{
    // Acquire both buckets. In a multithreaded app you could launch two
    // std::threads here and join them for parallel acquisition.
    req_bucket.acquire(1);
    tok_bucket.acquire(token_cost);
}

// ---------------------------------------------------------------------------
// RateLimiter (base)
// ---------------------------------------------------------------------------

RateLimiter::RateLimiter(int                        request_limit,
                          int                        token_limit,
                          std::shared_ptr<BaseStore> store,
                          std::string                namespace_key)
    : store_(store ? std::move(store) : std::make_shared<MemoryStore>())
    , request_bucket_(request_limit, store_, namespace_key + ":requests")
    , token_bucket_(token_limit,    store_, namespace_key + ":tokens")
{}

LimitGuard RateLimiter::limit(int token_cost) {
    return LimitGuard(request_bucket_, token_bucket_, token_cost);
}

// ---------------------------------------------------------------------------
// ChatRateLimiter
// ---------------------------------------------------------------------------

LimitGuard ChatRateLimiter::limit(const std::string&          model,
                                   const std::vector<Message>& messages,
                                   int                         max_tokens,
                                   int                         n)
{
    int cost = num_tokens_consumed_by_chat_request(model, messages, max_tokens, n);
    return RateLimiter::limit(cost);
}

// ---------------------------------------------------------------------------
// CompletionRateLimiter
// ---------------------------------------------------------------------------

LimitGuard CompletionRateLimiter::limit(const std::string& model,
                                         const std::string& prompt,
                                         int                max_tokens,
                                         int                n)
{
    int cost = num_tokens_consumed_by_completion_request(model, prompt, max_tokens, n);
    return RateLimiter::limit(cost);
}

// ---------------------------------------------------------------------------
// EmbeddingRateLimiter
// ---------------------------------------------------------------------------

LimitGuard EmbeddingRateLimiter::limit(const std::string& model,
                                        const std::string& input)
{
    int cost = num_tokens_consumed_by_embedding_request(model, input);
    return RateLimiter::limit(cost);
}

} // namespace limitless