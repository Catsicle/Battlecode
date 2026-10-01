import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from bc.deaths import classify
from bc.gitinfo import bot_hash
from bc.loops import detect_loop
from bc.parse import parse_deaths, parse_game_output, parse_points
from bc.registry import discover, select
from bc.stats import elo_diff, wilson


def test_runner_parsers():
    text = "team A wins after 12 rounds (last dragon)\nteam A points per turn: p50 1.0M p99 2.0M mean 1.2M max 3.0M (12 turns)\nround 3: bot 4 (team B) died: Hit Self"
    assert parse_game_output(text) == ("win", "A", 12, "last dragon")
    assert parse_points(text)["A"]["max_M"] == 3.0
    assert parse_deaths(text)[0]["cause"] == "Hit Self"


def test_wilson_edges():
    assert wilson(0, 0) == (0.0, 1.0)
    assert wilson(0, 10)[0] == 0
    assert wilson(10, 10)[1] == 1
    assert elo_diff(0.5) == 0


def test_loops_and_pearl_break():
    positions = ["a", "b"] * 4
    assert detect_loop(positions)[0]["period"] == 2
    assert detect_loop(positions, {3}) == []
    assert detect_loop(list("abcdef")) == []


def test_death_classifier():
    assert classify("Hit Wall") == "suicide"
    assert classify("Head to Head") == "deliberate_attack"
    assert classify("mysterious event") == "unclassified"


def test_registry_and_hash_invariance(tmp_path):
    bot = tmp_path / "bot"
    bot.mkdir()
    (bot / "bot.toml").write_text("[project]\n")
    (bot / "main.py").write_bytes(b"print(1)\r\n")
    first = bot_hash(bot)
    (bot / "main.py").write_bytes(b"print(1)\n")
    assert bot_hash(bot) == first
    (bot / "main.py").write_bytes(b"print(2)\n")
    assert bot_hash(bot) != first
