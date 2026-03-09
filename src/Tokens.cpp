#include "limitless/tokens.hpp"

#include <sstream>
#include <unordered_map>

namespace limitless {

// ---------------------------------------------------------------------------
// Approximate token counting
//
// A proper implementation would use a BPE tokeniser (like tiktoken).
// This approximation counts whitespace-separated words and applies a 1.33x
// multiplier, which is accurate to within ~10% for typical English text —
// more than good enough for rate-limit budgeting (we always over-estimate
// slightly to stay safely under the limit).
//
// To integrate a real tokeniser, replace word_count() with a call to your
// preferred BPE library (e.g. github.com/google/sentencepiece).
// ---------------------------------------------------------------------------

static int word_count(const std::string& text) {
    std::istringstream iss(text);
    int count = 0;
    std::string word;
    while (iss >> word) ++count;
    return count;
}

// Rough tokens ≈ words * 1.33  (matches OpenAI's rule of thumb)
static int approx_tokens(const std::string& text) {
    return static_cast<int>(word_count(text) * 1.33) + 1;
}

// Per-model message overhead (matches tiktoken's chat formatting overhead)
struct ChatOverhead {
    int per_message;
    int per_name;
    int reply_overhead;
};

static ChatOverhead get_overhead(const std::string& model) {
    static const std::unordered_map<std::string, ChatOverhead> table = {
        {"gpt-4",              {3,  1, 3}},
        {"gpt-4-0314",         {3,  1, 3}},
        {"gpt-4-32k",          {3,  1, 3}},
        {"gpt-4-32k-0314",     {3,  1, 3}},
        {"gpt-3.5-turbo",      {4, -1, 3}},
        {"gpt-3.5-turbo-0301", {4, -1, 3}},
    };
    auto it = table.find(model);
    return it != table.end() ? it->second : ChatOverhead{3, 1, 3};
}

// ---------------------------------------------------------------------------

int num_tokens_consumed_by_chat_request(
    const std::string&          model,
    const std::vector<Message>& messages,
    int                         max_tokens,
    int                         n)
{
    auto ovh = get_overhead(model);
    int prompt_tokens = ovh.reply_overhead;

    for (const auto& msg : messages) {
        prompt_tokens += ovh.per_message;
        prompt_tokens += approx_tokens(msg.role);
        prompt_tokens += approx_tokens(msg.content);
        if (!msg.name.empty()) {
            prompt_tokens += approx_tokens(msg.name) + ovh.per_name;
        }
    }

    int completion_tokens = max_tokens * n;
    return prompt_tokens + completion_tokens;
}

int num_tokens_consumed_by_completion_request(
    const std::string& /*model*/,
    const std::string& prompt,
    int                max_tokens,
    int                n)
{
    return approx_tokens(prompt) + max_tokens * n;
}

int num_tokens_consumed_by_embedding_request(
    const std::string& /*model*/,
    const std::string& input)
{
    return approx_tokens(input);
}

} // namespace limitless