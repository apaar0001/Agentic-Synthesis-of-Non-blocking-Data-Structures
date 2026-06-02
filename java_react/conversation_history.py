"""
conversation_history.py

Manages a growing list of LangChain BaseMessage objects representing the
full multi-turn conversation sent to NVIDIA NIM on every call.

Usage pattern
-------------
  history = ConversationHistory.from_dict_list(state.get("conversation_history", []))
  history.add_system(SYSTEM_PROMPT)   # idempotent
  history.add_user("Generate X...")
  response = llm.invoke(history.to_langchain_messages())
  history.add_assistant(response.content)
  return {"conversation_history": history.to_dict_list()}
"""

from __future__ import annotations

from typing import List

from langchain_core.messages import AIMessage, BaseMessage, HumanMessage, SystemMessage


def extract_text_from_response(response) -> str:
    """
    Safely extract a plain string from an llm.invoke() response.

    Some models (e.g. Claude via NVIDIA NIM) return response.content as a
    list of content blocks:  [{"type": "text", "text": "..."}, ...]
    Others return it as a plain string.  This handles both.
    """
    content = response.content if hasattr(response, "content") else str(response)
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for block in content:
            if isinstance(block, dict):
                parts.append(block.get("text") or block.get("content") or "")
            else:
                parts.append(str(block))
        return "".join(parts)
    return str(content)


class ConversationHistory:
    """Wrapper around a growing list of LangChain messages."""

    def __init__(self) -> None:
        self.messages: List[BaseMessage] = []

    # ------------------------------------------------------------------ #
    # Mutation helpers                                                      #
    # ------------------------------------------------------------------ #

    def add_system(self, text: str) -> None:
        """Add a SystemMessage — idempotent; only the first call takes effect."""
        if not any(isinstance(m, SystemMessage) for m in self.messages):
            self.messages.append(SystemMessage(content=text))

    def add_user(self, text: str) -> None:
        self.messages.append(HumanMessage(content=text))

    def add_assistant(self, text: str) -> None:
        self.messages.append(AIMessage(content=text))

    # ------------------------------------------------------------------ #
    # LLM integration                                                       #
    # ------------------------------------------------------------------ #

    def to_langchain_messages(self) -> List[BaseMessage]:
        """Return the raw message list to pass directly to llm.invoke(...)."""
        return self.messages

    # ------------------------------------------------------------------ #
    # Serialisation (for GraphState storage)                                #
    # ------------------------------------------------------------------ #

    def to_dict_list(self) -> List[dict]:
        """Serialise to JSON-safe dicts for storage in TypedDict GraphState."""
        role_map: dict = {
            SystemMessage: "system",
            HumanMessage: "user",
            AIMessage: "assistant",
        }
        return [
            {"role": role_map[type(m)], "content": m.content}
            for m in self.messages
        ]

    @classmethod
    def from_dict_list(cls, data: List[dict]) -> "ConversationHistory":
        """Deserialise from GraphState's stored list of dicts."""
        cls_map = {
            "system": SystemMessage,
            "user": HumanMessage,
            "assistant": AIMessage,
        }
        h = cls()
        for d in (data or []):
            role = d.get("role", "user")
            content = d.get("content", "")
            msg_cls = cls_map.get(role, HumanMessage)
            h.messages.append(msg_cls(content=content))
        return h

    # ------------------------------------------------------------------ #
    # Convenience                                                           #
    # ------------------------------------------------------------------ #

    def clear(self) -> None:
        self.messages = []

    def __len__(self) -> int:
        return len(self.messages)

    def __repr__(self) -> str:
        return f"ConversationHistory({len(self.messages)} turns)"
