test:
	uv run pytest -q

smoke:
	uv run bctool smoke

quick:
	uv run bctool quick

full:
	uv run bctool full --games $(GAMES)

leaderboard:
	uv run bctool elo

report:
	uv run bctool report results/latest
