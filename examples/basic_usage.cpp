#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#include "limitless/limiter.hpp"

// Simulated OpenAI response
struct ChatResponse {
    std::string content;
};

// Pretend this calls the real OpenAI API
ChatResponse fake_openai_call(const std::string& prompt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // simulate latency
    return {"Response to: " + prompt};
}

int main() {
    // Allow 10 requests and 1000 tokens per minute
    limitless::ChatRateLimiter rate_limiter(/*request_limit=*/10,
                                            /*token_limit=*/1000);

    std::vector<std::string> prompts = {
        "What is the capital of France?",
        "Explain quantum computing simply.",
        "Write a haiku about rain.",
    };

    for (const auto& prompt : prompts) {
        std::vector<limitless::Message> messages = {
            {"user", prompt, ""}
        };

        // Blocks here if either limit would be exceeded, then proceeds
        auto guard = rate_limiter.limit("gpt-4", messages, /*max_tokens=*/50);

        auto response = fake_openai_call(prompt);
        std::cout << "Q: " << prompt << "\n"
                  << "A: " << response.content << "\n\n";
    }

    return 0;
}