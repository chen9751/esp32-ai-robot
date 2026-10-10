"""Offline regression tests: LMS is mocked; no HA/weather/media actions."""
import unittest
from unittest.mock import patch
from server.agent import DEFAULT_CITY, execute_tool, parse_json_response, run, validate_intent


class IntentTests(unittest.TestCase):
    def test_plain_json(self):
        self.assertEqual(parse_json_response('{"action":"none"}'), {"action": "none"})

    def test_fenced_json(self):
        self.assertEqual(parse_json_response('```json\n{"action":"none"}\n```'),
                         {"action": "none"})

    def test_prose_rejected(self):
        with self.assertRaises(ValueError):
            parse_json_response('Done! {"action":"set_light"}')

    def test_array_rejected(self):
        with self.assertRaises(ValueError):
            parse_json_response('[]')

    def test_unknown_action(self):
        with self.assertRaises(ValueError):
            validate_intent({"action": "shell", "command": "echo hi"})

    def test_extra_fields_rejected(self):
        with self.assertRaises(ValueError):
            validate_intent({"action": "set_light", "room": "living_room",
                             "brightness_pct": 30, "command": "x"})

    def test_default_city(self):
        self.assertEqual(validate_intent({"action": "get_weather"})["city"], DEFAULT_CITY)
        self.assertEqual(validate_intent({"action": "get_weather", "city": "Beijing"})["city"],
                         "Beijing")

    def test_light_valid(self):
        self.assertEqual(validate_intent({"action": "set_light", "room": "living_room",
                                          "brightness_pct": 30})["brightness_pct"], 30)

    def test_invalid_brightness(self):
        for level in (True, -1, 101, "30"):
            with self.subTest(level=level), self.assertRaises(ValueError):
                validate_intent({"action": "set_light", "room": "living_room",
                                 "brightness_pct": level})

    def test_unconfigured_room(self):
        with self.assertRaises(ValueError):
            validate_intent({"action": "set_light", "room": "unknown",
                             "brightness_pct": 30})

    def test_unknown_tool_denied(self):
        self.assertFalse(execute_tool("shell", {"command": "echo hi"})["ok"])

    def test_weather_mock_no_fake_temperature(self):
        data = execute_tool("get_weather", {})
        self.assertTrue(data["simulated"])
        self.assertEqual(data["city"], DEFAULT_CITY)
        self.assertNotIn("temperature_c", data)

    @patch("server.agent.completion")
    def test_light_routing(self, mock):
        mock.return_value = {"content": '```json\n{"action":"set_light","room":"living_room","brightness_pct":30}\n```'}
        response = run("把客厅灯调到30%")
        self.assertEqual(response["tool_trace"][0]["tool"], "set_light")
        self.assertFalse(response["verified"])
        self.assertIn("simulation", response["answer"])

    @patch("server.agent.completion")
    def test_weather_routing(self, mock):
        mock.return_value = {"content": '{"action":"get_weather","city":"Kunming, Wuhua"}'}
        response = run("今天天气如何")
        self.assertEqual(response["tool_trace"][0]["arguments"]["city"], DEFAULT_CITY)
        self.assertFalse(response["verified"])

    @patch("server.agent.completion")
    def test_story_routing(self, mock):
        mock.return_value = {"content": '{"action":"search_story","query":"Peter Rabbit"}'}
        response = run("找一个彼得兔故事")
        self.assertEqual(response["tool_trace"][0]["tool"], "search_story")
        self.assertFalse(response["verified"])

    @patch("server.agent.completion")
    def test_chat(self, mock):
        mock.return_value = {"content": '{"action":"chat","answer":"Hello!"}'}
        response = run("你好")
        self.assertEqual(response["answer"], "Hello!")
        self.assertEqual(response["tool_trace"], [])

    @patch("server.agent.completion")
    def test_hallucinated_success_denied(self, mock):
        mock.return_value = {"content": '{"action":"chat","answer":"I have adjusted your light."}'}
        response = run("把客厅灯调到30%")
        self.assertFalse(response["verified"])
        self.assertNotIn("adjusted", response["answer"])

    @patch("server.agent.completion")
    def test_invalid_json_denied(self, mock):
        mock.return_value = {"content": "I adjusted your light."}
        self.assertFalse(run("把客厅灯调到30%")["verified"])

    @patch("server.agent.completion")
    def test_missing_parameters_denied(self, mock):
        mock.return_value = {"content": '{"action":"set_light","room":"living_room"}'}
        response = run("把客厅灯调亮")
        self.assertFalse(response["verified"])
        self.assertEqual(response["tool_trace"], [])

    @patch("server.agent.completion")
    def test_none(self, mock):
        mock.return_value = {"content": '{"action":"none"}'}
        self.assertFalse(run("打开灯")["verified"])


if __name__ == "__main__":
    unittest.main()
