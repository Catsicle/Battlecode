import sys

from bctool.cli import main


def _compat(argv: list[str]) -> list[str]:
	if "--list-bots" in argv:
		return ["list"]
	result: list[str] = ["run"]
	index = 0
	while index < len(argv):
		value = argv[index]
		if value == "--output-dir":
			result.extend(["--out", argv[index + 1]])
			index += 2
		elif value in {"--no-logs", "--no-indicator", "--no-draw"}:
			index += 1
		elif value in {"--maps", "--opponents"}:
			result.append(value)
			index += 1
			values = []
			while index < len(argv) and not argv[index].startswith("--"):
				values.append(argv[index])
				index += 1
			result.append(",".join(values))
		else:
			result.append(value)
			index += 1
	return result

if __name__ == "__main__":
	raise SystemExit(main(_compat(sys.argv[1:])))
