from pathlib import Path
import sys

from bctool.elo import write

if __name__ == "__main__":
	raise SystemExit(print(write(Path(__file__).resolve().parents[1])) or 0)
