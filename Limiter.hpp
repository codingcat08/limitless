/*
rate limiter using sliding window log algo 
window - default(60 sec)
when a req comes it throws entries older than now - window and thenn calculates 
sum of window tokens if it is less than limit it allows req to enter else sleeps 
for no. of sec to exclude 1st entry and then retries later again 
and then appends (timestamp,cost) to the window again 
*/
#pragma once

#include "store.hpp"
#include "tokens.hpp"

#include <functional>
#include <memory>
#include <string>

namespace limitless {

class SlidingWindowLimiter {
public:
    SlidingWindowLimiter(int limit,
                         std::shared_ptr<BaseStore> store,
                         std::string key,
                         double window_seconds = 60.0);

    /// Block the calling thread until `cost` units can be admitted.
    void acquire(int cost = 1);

private:
    int                        limit_;
    std::shared_ptr<BaseStore> store_;
    std::string                key_;
    double                     window_;
};

class LimitGuard {
public:
    explicit LimitGuard(SlidingWindowLimiter& req_bucket,
                        SlidingWindowLimiter& tok_bucket,
                        int token_cost);
    // Non-copyable, non-movable — use in a local scope only
    LimitGuard(const LimitGuard&)            = delete;
    LimitGuard& operator=(const LimitGuard&) = delete;
};

class RateLimiter {
public:
    RateLimiter(int request_limit,
                int token_limit,
                std::shared_ptr<BaseStore> store = nullptr,
                std::string namespace_key        = "limitless");

    virtual ~RateLimiter() = default;

    /// Block until both request and token budgets allow this call.
    /// Returns a LimitGuard (RAII) — keep it in scope for the duration
    /// of your API call (though for rate limiting, construction is enough).
    ///
    /// Usage:
    ///   auto guard = rate_limiter.limit(params);
    ///   auto response = openai_client.chat(params);
    LimitGuard limit(int token_cost);

    /// Wrap a callable: blocks until rate-limits allow, then calls fn().
    template <typename Fn>
    auto call(int token_cost, Fn&& fn) -> decltype(fn()) {
        auto guard = limit(token_cost);
        return fn();
    }

protected:
    virtual int count_tokens_impl() const { return 0; }

    std::shared_ptr<BaseStore> store_;
    SlidingWindowLimiter       request_bucket_;
    SlidingWindowLimiter       token_bucket_;
};


class ChatRateLimiter : public RateLimiter {
public:
    using RateLimiter::RateLimiter;

    /// Block until rate-limits allow, passing full chat params for token counting.
    LimitGuard limit(const std::string&          model,
                     const std::vector<Message>& messages,
                     int                         max_tokens = 15,
                     int                         n          = 1);
};

class CompletionRateLimiter : public RateLimiter {
public:
    using RateLimiter::RateLimiter;

    LimitGuard limit(const std::string& model,
                     const std::string& prompt,
                     int                max_tokens = 15,
                     int                n          = 1);
};

class EmbeddingRateLimiter : public RateLimiter {
public:
    using RateLimiter::RateLimiter;

    LimitGuard limit(const std::string& model,
                     const std::string& input);
};

} // namespace limitless