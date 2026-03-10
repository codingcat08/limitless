"""
Example: using the C++-backed limitless from Python.

Build first:
    pip install pybind11
    python setup.py build_ext --inplace

Then run:
    python examples/basic_usage.py
"""

import openai
import limitless   # <-- this is your compiled C++ extension

# 200 requests/min, 40,000 tokens/min
rate_limiter = limitless.ChatRateLimiter(
    request_limit=200,
    token_limit=40_000,
)

messages = [
    limitless.Message(role="user", content="What is the capital of France?")
]

# --- Option 1: context manager ---
with rate_limiter.limit("gpt-4", messages, max_tokens=100):
    response = openai.chat.completions.create(
        model="gpt-4",
        messages=[{"role": m.role, "content": m.content} for m in messages],
        max_tokens=100,
    )
    print(response.choices[0].message.content)


# --- Option 2: assign guard to variable ---
guard = rate_limiter.limit("gpt-4", messages, max_tokens=100)
response = openai.chat.completions.create(
    model="gpt-4",
    messages=[{"role": m.role, "content": m.content} for m in messages],
    max_tokens=100,
)
print(response.choices[0].message.content)


# --- Option 3: token counting only ---
n = limitless.num_tokens_consumed_by_chat_request(
    model="gpt-4",
    messages=messages,
    max_tokens=100,
)
print(f"Estimated tokens: {n}")