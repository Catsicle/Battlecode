from pathlib import Path
import sys

from bctool.report import write

if len(sys.argv) != 2:
    raise SystemExit("usage: uv run scripts/analyze.py RUN_DIR")
raise SystemExit(print(write(Path(sys.argv[1]))) or 0)
