from __future__ import annotations

import sys
from pathlib import Path

from .app import run_app


def find_repo_root() -> Path:
    current = Path(__file__).resolve()
    for parent in current.parents:
        if (parent / "configs").exists() and (parent / "src").exists():
            return parent
    return Path.cwd()


def main() -> int:
    return run_app(find_repo_root())


if __name__ == "__main__":
    sys.exit(main())

