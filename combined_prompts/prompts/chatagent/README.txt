=== CHAT-AGENT PROMPTS ===

The chat-agent experiments use the IDENTICAL C++ zero-shot prompts
(system prompt + per-data-structure user prompt) documented in the
zeroshot/ directory (cpp_*.txt files).

These prompts were submitted verbatim to:
  - Claude Sonnet 4.6 (Anthropic)
  - Gemini (Google)
  - GPT-5.5 (OpenAI)

via their respective web chat interfaces with default settings.
Each model was queried N=10 times per data structure.

No additional prompt engineering was performed.
See: zeroshot/cpp_*.txt for the exact prompts used.
