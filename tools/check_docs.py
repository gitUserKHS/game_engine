"""Check repository text encoding and local Markdown links."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    ".c",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".json",
    ".md",
    ".txt",
    ".yaml",
    ".yml",
}
IGNORED_DIRECTORIES = {".git", ".vs", "build", "out"}
REQUIRED_DOCUMENTS = {
    "README.md",
    "CONTRIBUTING.md",
    "CHANGELOG.md",
    "docs/00_START_HERE.md",
    "docs/01_BUILD_AND_RUN.md",
    "docs/02_CODE_READING_ORDER.md",
    "docs/03_EDITOR_QUICKSTART.md",
    "docs/AI_VISUAL_TESTING.md",
    "docs/ARCHITECTURE.md",
    "docs/GLOSSARY.md",
    "docs/DEVELOPMENT.md",
    "docs/GIT_WORKFLOW.md",
    "docs/ROADMAP.md",
}
LINK_PATTERN = re.compile(r"!?\[[^\]]*]\(([^)]+)\)")


def is_ignored(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    return any(
        part in IGNORED_DIRECTORIES or part.startswith("build-")
        for part in relative.parts
    )


def text_files() -> list[Path]:
    files: list[Path] = []
    for path in ROOT.rglob("*"):
        if not path.is_file() or is_ignored(path):
            continue
        if path.name == "CMakeLists.txt" or path.suffix.lower() in TEXT_SUFFIXES:
            files.append(path)
    return files


def check_utf8(files: list[Path]) -> list[str]:
    errors: list[str] = []
    for path in files:
        try:
            path.read_text(encoding="utf-8", errors="strict")
        except UnicodeError as error:
            errors.append(f"{path.relative_to(ROOT)} is not valid UTF-8: {error}")
    return errors


def local_target(raw_target: str, source: Path) -> Path | None:
    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    target = target.split(maxsplit=1)[0]
    if not target or target.startswith("#"):
        return None
    if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", target):
        return None

    path_text = unquote(target.split("#", maxsplit=1)[0])
    if not path_text:
        return None
    if path_text.startswith("/"):
        return ROOT / path_text.lstrip("/")
    return source.parent / path_text


def check_links(files: list[Path]) -> list[str]:
    errors: list[str] = []
    for source in (path for path in files if path.suffix.lower() == ".md"):
        text = source.read_text(encoding="utf-8")
        for line_number, line in enumerate(text.splitlines(), start=1):
            for match in LINK_PATTERN.finditer(line):
                target = local_target(match.group(1), source)
                if target is not None and not target.resolve().exists():
                    relative = source.relative_to(ROOT)
                    errors.append(
                        f"{relative}:{line_number}: missing link target "
                        f"{match.group(1)}"
                    )
    return errors


def main() -> int:
    errors = [
        f"Missing required document: {path}"
        for path in sorted(REQUIRED_DOCUMENTS)
        if not (ROOT / path).exists()
    ]
    files = text_files()
    errors.extend(check_utf8(files))
    errors.extend(check_links(files))

    if errors:
        print("Documentation checks failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    markdown_count = sum(path.suffix.lower() == ".md" for path in files)
    print(
        f"Documentation checks passed: {len(files)} UTF-8 text files, "
        f"{markdown_count} Markdown files."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
