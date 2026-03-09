#pragma once

#include <string>
#include <vector>
#include <map>

namespace limitless {

struct Message {
    std::string role;
    std::string content;
    std::string name;  // optional
};

/// Estimate max tokens consumed by a chat completion request.
/// Includes prompt tokens + max_tokens * n (worst-case completion).
int num_tokens_consumed_by_chat_request(
    const std::string&          model,
    const std::vector<Message>& messages,
    int                         max_tokens = 15,
    int                         n          = 1
);

/// Estimate max tokens consumed by a text completion request.
int num_tokens_consumed_by_completion_request(
    const std::string& model,
    const std::string& prompt,
    int                max_tokens = 15,
    int                n          = 1
);

/// Estimate tokens consumed by an embedding request.
int num_tokens_consumed_by_embedding_request(
    const std::string& model,
    const std::string& input
);

} // namespace limitless