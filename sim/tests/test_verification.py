#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""自定义终端上行指令验证器单测。"""

import unittest

from server.verification import TerminalVerifier


class FakeClock:
    def __init__(self):
        self.value = 0.0

    def __call__(self):
        return self.value

    def advance(self, seconds):
        self.value += float(seconds)


class TerminalVerifierTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock()
        self.verifier = TerminalVerifier(clock=self.clock)

    def test_allowed_topics_pass(self):
        self.verifier.record("KeyboardMouseControl", b"\x00" * 12)
        self.verifier.record("CustomControl", b"\x00" * 10)
        self.verifier.record("CommonCommand", b"\x00" * 4)
        summary = self.verifier.summary()
        self.assertTrue(summary["pass"])
        self.assertEqual(summary["violationCount"], 0)
        self.assertEqual(summary["counts"]["KeyboardMouseControl"], 1)

    def test_custom_control_size_limit(self):
        problems = self.verifier.record("CustomControl", b"\x00" * 61)
        self.assertEqual(len(problems), 1)
        self.assertIn("60", problems[0])
        self.assertFalse(self.verifier.summary()["pass"])

    def test_keyboard_rate_limit(self):
        for _ in range(84):
            problems = self.verifier.record("KeyboardMouseControl", b"\x00")
        self.assertTrue(problems)
        self.assertIn("75Hz", problems[0])

    def test_command_rate_limit(self):
        for _ in range(12):
            problems = self.verifier.record("AssemblyCommand", b"\x00")
        self.assertTrue(problems)
        self.assertIn("10Hz", problems[0])

    def test_unknown_topic_is_flagged(self):
        problems = self.verifier.record("AirSupportCommand", b"\x00")
        self.assertEqual(len(problems), 1)
        self.assertIn("2027", problems[0])

    def test_rate_window_expires(self):
        for _ in range(84):
            self.verifier.record("KeyboardMouseControl", b"\x00")
        self.clock.advance(2.0)
        problems = self.verifier.record("KeyboardMouseControl", b"\x00")
        self.assertEqual(problems, [])

    def test_reset_clears_state(self):
        self.verifier.record("AssemblyCommand", b"\x00")
        self.verifier.reset()
        summary = self.verifier.summary()
        self.assertEqual(summary["counts"], {})
        self.assertTrue(summary["pass"])


if __name__ == "__main__":
    unittest.main()
