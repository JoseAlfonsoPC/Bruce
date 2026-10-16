"""Check relative file links in maintained project documentation, offline."""

from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


def broken_links(paths):
    errors = []
    for path in paths:
        content = re.sub(r"```.*?```", "", path.read_text(encoding="utf-8"), flags=re.S)
        for target in re.findall(r"!?\[[^\]]*\]\(([^)]+)\)", content):
            target = target.strip().strip("<>")
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            destination = path.parent / unquote(parsed.path)
            if not destination.exists():
                errors.append(f"{path.relative_to(ROOT)}: {target}")
    return errors


if __name__ == "__main__":
    paths = sorted((ROOT / "docs").glob("*.md"))
    paths += [ROOT / name for name in ("README.md", "LAYOUT.md", "CONTRIBUTING.md", "2.0_road_path.md")]
    errors = broken_links(paths)
    if errors:
        raise SystemExit("Broken documentation links:\n" + "\n".join(errors))
    print(f"Documentation links OK ({len(paths)} files)")
