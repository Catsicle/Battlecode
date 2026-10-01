test:
	python -m pytest -q

smoke:
	python -m bc smoke

quick:
	python -m bc quick

full:
	python -m bc full --games $(GAMES)

leaderboard:
	python -m bc elo

report:
	python -m bc report results/latest
