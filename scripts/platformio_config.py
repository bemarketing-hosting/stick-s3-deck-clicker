"""PlatformIO pre-script: validate YAML in PlatformIO's own Python process."""
import sys
from pathlib import Path

Import("env")  # noqa: F821 -- provided by PlatformIO/SCons
project_dir = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
sys.path.insert(0, str(project_dir / "scripts"))
from generate_config import generate

try:
    generate(project_dir / "config.yaml", project_dir / "include/generated_config.h")
except (OSError, ValueError) as exc:
    raise SystemExit(f"Configuration error: {exc}") from exc
