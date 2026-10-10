"""Offline safety tests: no actual LMS, HA, weather, or media requests."""
import json
import unittest
from unittest.mock import patch
from server.agent import execute_tool, run


class ToolTests(unittest.TestCase):
    def test_weather(self):
        self.assertTrue(execute_tool("get_weather", {"city": "Kunming"})["simulated"])

    def test_light_bounds(self):
        self.assertFalse(execute_tool("set_light", {"room": "living", "brightness_pct": 101})["ok"])
        self.assertFalse(execute_tool("set_light", {"room": "living", "brightness_pct": True})["ok"])
        self.assertTrue(execute_tool("set_light", {"room": "living", "brightness_pct": 30})["simulated"])

    def test_unknown_tool_denied(self):
        self.assertFalse(execute_tool("shell", {"command": "rm -rf /"})["ok"])

    @patch("server.agent.completion")
    def test_tool_loop(self, mocked):
        mocked.side_effect = [
            {"content": None, "tool_calls": [{"id": "call1", "type": "function",
              "function": {"name": "get_weather", "arguments": json.dumps({"city": "Kunming"})}}]},
            {"content": "It's sunny in Kunming.", "tool_calls": []},
        ]
        response = run("昆明天气")
        self.assertEqual(response["tool_trace"][0]["tool"], "get_weather")
        self.assertIn("sunny", response["answer"])

    @patch("server.agent.completion")
    def test_plain_chat(self, mocked):
        mocked.return_value = {"content": "Hello!", "tool_calls": []}
        self.assertEqual(run("你好")["answer"], "Hello!")


if __name__ == "__main__":
    unittest.main()
