"""自定义终端上行指令验证器。

模拟器作为“可验证后端”，除了按 2027 协议下行数据，还需要检查自定义终端上行
的指令是否符合协议约束。本模块只做被动记录与校验，不修改状态：

- topic 必须在 2027 自定义终端指令表内；
- CustomControl 载荷不超过 60 字节；
- KeyboardMouseControl / CustomControl 频率上限 75Hz；
- 其余赛事指令（CommonCommand、DartCommand 等）频率上限 10Hz。

结果通过 ``GET /api/verify/summary`` 暴露，便于联调时直接判断“这台上行是否合规”。
"""

from __future__ import annotations

import threading
import time
from collections import defaultdict, deque

# 2027 协议 2.1 指令概览中的上行指令（自定义终端 → 服务器/机器人）。
KEYMOUSE_TOPICS = {
    "KeyboardMouseControl": 75,
    "CustomControl": 75,
}
COMMAND_TOPICS = {
    "MapClickCmd": 10,
    "AssemblyCommand": 10,
    "RobotPerformanceSelectionCommand": 10,
    "CommonCommand": 10,
    "RuneActivateCommand": 10,
    "DartCommand": 10,
    "SentryCtrlCommand": 10,
}
ALLOWED_TOPICS = tuple(KEYMOUSE_TOPICS) + tuple(COMMAND_TOPICS)
MAX_CUSTOM_CONTROL_BYTES = 60
# 频率判定窗口与容差：允许 10% 的调度抖动，避免把边界值误判为超频。
RATE_WINDOW_SEC = 1.0
RATE_TOLERANCE = 1.1


class TerminalVerifier:
    """记录并校验自定义终端上行指令。"""

    def __init__(self, clock=time.monotonic):
        self._clock = clock
        self._lock = threading.Lock()
        self.reset()

    def reset(self) -> None:
        with self._lock:
            self._started_at = self._clock()
            self._counts: dict[str, int] = defaultdict(int)
            self._timestamps: dict[str, deque] = defaultdict(deque)
            self._violations: list[dict] = []
            self._events: deque = deque(maxlen=500)
            self._bytes_total = 0

    def record(self, topic, payload, detail: str | None = None) -> list[str]:
        """记录一条上行报文，返回本条产生的违规描述（无违规则为空列表）。"""
        now = self._clock()
        payload_len = len(payload or b"")
        problems: list[str] = []

        with self._lock:
            self._counts[topic] += 1
            self._bytes_total += payload_len
            stamps = self._timestamps[topic]
            stamps.append(now)
            self._trim(stamps, now)

            if topic not in ALLOWED_TOPICS:
                problems.append(f"2027 协议未定义的上行 topic: {topic}")
            elif topic == "CustomControl" and payload_len > MAX_CUSTOM_CONTROL_BYTES:
                problems.append(
                    f"CustomControl 载荷 {payload_len} 字节超过 "
                    f"{MAX_CUSTOM_CONTROL_BYTES} 字节上限"
                )

            limit = KEYMOUSE_TOPICS.get(topic) or COMMAND_TOPICS.get(topic)
            if limit is not None and len(stamps) > limit * RATE_TOLERANCE:
                problems.append(
                    f"{topic} 频率 {len(stamps):.0f}Hz 超过 {limit}Hz 上限"
                )

            event = {
                "topic": topic,
                "bytes": payload_len,
                "violations": problems,
            }
            if detail:
                event["detail"] = detail
            self._events.append(event)

            for problem in problems:
                self._violations.append({"topic": topic, "message": problem})
            if len(self._violations) > 200:
                self._violations = self._violations[-200:]

        return problems

    def summary(self) -> dict:
        with self._lock:
            now = self._clock()
            rates = {
                topic: len(stamps) / RATE_WINDOW_SEC
                for topic, stamps in self._timestamps.items()
            }
            return {
                "available": True,
                "startedAt": self._started_at,
                "now": now,
                "durationSec": max(0.0, now - self._started_at),
                "allowedTopics": list(ALLOWED_TOPICS),
                "maxCustomControlBytes": MAX_CUSTOM_CONTROL_BYTES,
                "counts": dict(self._counts),
                "ratesHz": {k: round(v, 1) for k, v in rates.items()},
                "bytesTotal": self._bytes_total,
                "violationCount": len(self._violations),
                "violations": list(self._violations[-50:]),
                "pass": not self._violations,
                "recentEvents": list(self._events)[-50:],
            }

    def _trim(self, stamps: deque, now: float) -> None:
        while stamps and stamps[0] < now - RATE_WINDOW_SEC:
            stamps.popleft()
