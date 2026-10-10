"""Run from repository root: python3 -m server.cli '查询昆明天气'"""
import json
import sys
from .agent import run

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 -m server.cli 'your question'", file=sys.stderr)
        raise SystemExit(2)
    try:
        result = run(" ".join(sys.argv[1:]))
    except (RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1) from error
    print(json.dumps(result, ensure_ascii=False, indent=2))

if __name__ == "__main__":
    main()
