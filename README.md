# limitless

> Precise, thread-safe rate limiting for the OpenAI API — powered by the **Sliding Window Log** algorithm.
> Written in C++ for performance, with Python bindings via **pybind11** so it works in both ecosystems.

---

## Project Structure

```
limitless/
├── include/limitless/
│   ├── limiter.hpp       ← rate limiter classes
│   ├── store.hpp         ← memory store (thread-safe)
│   └── tokens.hpp        ← token counting
├── src/
│   ├── limiter.cpp
│   ├── store.cpp
│   ├── tokens.cpp
│   └── bindings.cpp      ← pybind11 Python bindings
├── examples/
│   ├── basic_usage.cpp   ← C++ example
│   └── basic_usage.py    ← Python example
├── CMakeLists.txt        ← C++ build
└── setup.py              ← Python build (pip)
```

---

## Requirements

- C++17 or later
- CMake 3.15+
- `pybind11` *(optional, for Python bindings)*

---

## C++ Usage

### Build

```bash
cmake -B build
cmake --build build
./build/basic_usage
```

### Code

```cpp
#include "limitless/limiter.hpp"

// 200 requests/min, 40,000 tokens/min
limitless::ChatRateLimiter rate_limiter(200, 40000);

std::vector<limitless::Message> messages = {
    {"user", "Hello!", ""}
};

// Blocks until both limits allow, then proceeds
auto guard = rate_limiter.limit("gpt-4", messages, /*max_tokens=*/50);
auto response = openai_client.chat(messages);
```

---

## Python Usage

### Build & Install

```bash
pip install pybind11
pip install .
```

For local development (builds in-place, no install):

```bash
python setup.py build_ext --inplace
```

### Code

```python
import limitless

rate_limiter = limitless.ChatRateLimiter(request_limit=200, token_limit=40_000)
messages = [limitless.Message(role="user", content="Hello!")]

# Option 1: context manager
with rate_limiter.limit("gpt-4", messages, max_tokens=100):
    response = openai.chat.completions.create(...)

# Option 2: assign guard to variable
guard = rate_limiter.limit("gpt-4", messages, max_tokens=100)
response = openai.chat.completions.create(...)

# Token counting only
n = limitless.num_tokens_consumed_by_chat_request(
    model="gpt-4",
    messages=messages,
    max_tokens=100,
)
```

### Why faster than pure Python?

- Blocking (`sleep_for`) happens **inside C++**, releasing Python's GIL while waiting
- Other Python threads run freely while one is being rate-limited — no event loop needed
- `std::mutex` locking is ~10x faster than Python's threading primitives

---

## How It Works — Sliding Window Log

On each request:

1. **Fetch** all log entries from the past 60 s, pruning older ones
2. **Sum** the total cost of those entries
3. If `used + cost > limit` → sleep precisely until the oldest entry expires
4. Otherwise **append** the new entry and proceed

Two independent buckets run simultaneously — one for **requests**, one for **tokens**. Whichever fills up first acts as the bottleneck, matching OpenAI's actual behaviour exactly.

```
limit() called
      ↓
sliding window checked
      ↓
under limit?  ──No──→  sleep until oldest entry expires  ──→  retry
      │
     Yes
      ↓
entry appended to log
      ↓
LimitGuard created  ←─── acquisition happens here (RAII)
      ↓
your API call runs
      ↓
guard goes out of scope
      ↓
destructor runs automatically
```

---

## RAII — How LimitGuard Works

This pattern is called **RAII** (Resource Acquisition Is Initialization):

- **Resource acquired** when the object is created
- **Resource released** when the object is destroyed (goes out of scope)

You see this everywhere in the C++ standard library:

```cpp
std::lock_guard<std::mutex> lock(m);
// mutex unlocks automatically when lock goes out of scope
```

`LimitGuard` works the same way — the rate-limit slot is acquired on construction, and cleanup is automatic. No manual `release()` call needed, no risk of forgetting to release.

```cpp
{
    auto guard = rate_limiter.limit("gpt-4", messages, 50);
    // rate limit acquired here ↑

    auto response = openai_client.chat(messages);

} // ← guard destroyed here, slot released automatically
```

---

## Supported Models

| Limiter class          | Models                                              |
|------------------------|-----------------------------------------------------|
| `ChatRateLimiter`      | `gpt-4`, `gpt-4-32k`, `gpt-3.5-turbo`, and variants |
| `CompletionRateLimiter`| `text-davinci-003`, `text-curie-001`, and variants  |
| `EmbeddingRateLimiter` | `text-embedding-ada-002`                            |

---

## Token Counting

Token counting is available independently of rate limiting:

```cpp
#include "limitless/tokens.hpp"

// Chat
int n = limitless::num_tokens_consumed_by_chat_request(
    "gpt-4", messages, /*max_tokens=*/50
);

// Completion
int n = limitless::num_tokens_consumed_by_completion_request(
    "text-davinci-003", "Once upon a time", /*max_tokens=*/100
);

// Embedding
int n = limitless::num_tokens_consumed_by_embedding_request(
    "text-embedding-ada-002", "Hello world"
);
```

---

## Adding Redis Support (Coming Soon)

The store layer is designed for pluggability. To add distributed support, implement `BaseStore`:

```cpp
class RedisStore : public limitless::BaseStore {
    // implement get_log() and append() using hiredis sorted sets
};

auto store = std::make_shared<RedisStore>("localhost", 6379);
limitless::ChatRateLimiter limiter(200, 40000, store);
```

---

## Integrating into Another CMake Project

```cmake
find_package(limitless REQUIRED)
target_link_libraries(your_target PRIVATE limitless::limitless)
```

```cpp
#include "limitless/limiter.hpp"
```