"""Compatibility entry point for the reorganized Battlecode CLI."""

import sys

from scripts.tournament import _compat
from tools.bc.cli import main


if __name__ == "__main__":
    raise SystemExit(main(_compat(sys.argv[1:])))
