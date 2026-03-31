#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent

RIVE_RUNTIME_PATH = "3rdparty/rive-cpp"
RIVE_RUNTIME_URL = "https://github.com/rive-app/rive-runtime.git"
RIVE_RUNTIME_REV = "bf4e7e407e5214ecb72a310cba518c601f87754a"
RIVE_RUNTIME_CHECK = "include/rive/file.hpp"
RIVE_RUNTIME_ENV = "RIVEQT_RIVE_RUNTIME_REV"

PREMAKE_DEPENDENCY_SPECS = [
  {
    "premake": "dependencies/premake5_harfbuzz_v2.lua",
    "variable": "harfbuzz",
    "path": "3rdparty/harfbuzz",
    "check": "src/harfbuzz.cc",
  },
  {
    "premake": "dependencies/premake5_sheenbidi_v2.lua",
    "variable": "sheenbidi",
    "path": "3rdparty/SheenBidi",
    "check": [
      "Headers/SheenBidi.h",
      "Headers/SheenBidi/SheenBidi.h",
    ],
  },
  {
    "premake": "dependencies/premake5_yoga_v2.lua",
    "variable": "yoga",
    "path": "3rdparty/yoga",
    "check": "yoga/Yoga.h",
  },
  {
    "premake": "scripting/premake5.lua",
    "variable": "luau",
    "path": "3rdparty/luau",
    "check": "VM/include/lua.h",
  },
  {
    "premake": "scripting/premake5.lua",
    "variable": "libhydrogen",
    "path": "3rdparty/libhydrogen",
    "check": [
      "libhydrogen.c",
      "hydrogen.c",
    ],
  },
  {
    "premake": "renderer/premake5_pls_renderer.lua",
    "variable": "dx12_headers",
    "path": "3rdparty/directx-headers",
    "check": [
      "include/d3dx12.h",
      "include/directx/d3dx12.h",
    ],
  },
]

PIP_SOURCES = [
  {
    "path": "3rdparty/ply",
    "package": "ply==3.11",
  },
]

PREMAKE_GITHUB_PATTERN = re.compile(
  r"(?P<variable>[A-Za-z0-9_]+)\s*=\s*dependency\.github\(\s*['\"](?P<repo>[^'\"]+)['\"]\s*,\s*['\"](?P<ref>[^'\"]+)['\"]\s*\)"
)


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd or ROOT, check=True)


def dependency_ready(target: Path, entry: dict[str, object]) -> bool:
  check = entry.get("check")
  if not check:
    return target.exists()
  if isinstance(check, list):
    return any((target / item).exists() for item in check)
  return (target / str(check)).exists()


def resolve_rive_runtime_rev() -> str:
  env_rev = os.getenv(RIVE_RUNTIME_ENV)
  if env_rev:
    return env_rev
  return RIVE_RUNTIME_REV


def rive_runtime_entry() -> dict[str, object]:
  return {
    "path": RIVE_RUNTIME_PATH,
    "url": RIVE_RUNTIME_URL,
    "rev": resolve_rive_runtime_rev(),
    "check": RIVE_RUNTIME_CHECK,
  }


def parse_premake_dependency(
  rive_cpp_dir: Path,
  premake_path: str,
  variable: str,
  local_path: str,
  check: str | list[str],
) -> dict[str, object]:
  text = (rive_cpp_dir / premake_path).read_text(encoding="utf-8")
  for match in PREMAKE_GITHUB_PATTERN.finditer(text):
    if match.group("variable") != variable:
      continue
    repo = match.group("repo")
    ref = match.group("ref")
    return {
      "path": local_path,
      "url": f"https://github.com/{repo}.git",
      "rev": ref,
      "check": check,
    }

  raise RuntimeError(f"Could not derive {variable} from {premake_path}")


def gather_rive_dependencies(rive_cpp_dir: Path) -> list[dict[str, object]]:
  entries: list[dict[str, object]] = []
  for spec in PREMAKE_DEPENDENCY_SPECS:
    entries.append(
      parse_premake_dependency(
        rive_cpp_dir,
        str(spec["premake"]),
        str(spec["variable"]),
        str(spec["path"]),
        spec["check"],
      )
    )
  return entries


def apply_dependency_compatibility(target: Path, entry: dict[str, object]) -> None:
  if entry["path"] == "3rdparty/luau":
    lua_header = target / "VM" / "include" / "lua.h"
    if lua_header.exists():
      text = lua_header.read_text(encoding="utf-8")
      if "lua_pushvector2" not in text:
        needle = """#if LUA_VECTOR_SIZE == 4\nLUA_API void lua_pushvector(lua_State* L, float x, float y, float z, float w);\n#else\nLUA_API void lua_pushvector(lua_State* L, float x, float y, float z);\n#endif\n"""
        replacement = needle + """#ifndef lua_pushvector2\n#if LUA_VECTOR_SIZE == 4\n#define lua_pushvector2(L, x, y) lua_pushvector(L, x, y, 0.0f, 0.0f)\n#else\n#define lua_pushvector2(L, x, y) lua_pushvector(L, x, y, 0.0f)\n#endif\n#endif\n"""
        if needle in text:
          lua_header.write_text(text.replace(needle, replacement), encoding="utf-8")
    return

  if entry["path"] != "3rdparty/libhydrogen":
    return

  hydrogen_c = target / "hydrogen.c"
  libhydrogen_c = target / "libhydrogen.c"
  if hydrogen_c.exists() and not libhydrogen_c.exists():
    shutil.copy2(hydrogen_c, libhydrogen_c)

  hydrogen_h = target / "hydrogen.h"
  libhydrogen_h = target / "libhydrogen.h"
  if hydrogen_h.exists() and not libhydrogen_h.exists():
    shutil.copy2(hydrogen_h, libhydrogen_h)


def ensure_git_dependency(entry: dict[str, object], refresh: bool) -> None:
  target = ROOT / str(entry["path"])
  name = str(entry["path"])
  if target.exists() and refresh:
    shutil.rmtree(target)

  if target.exists():
    apply_dependency_compatibility(target, entry)

  if target.exists() and dependency_ready(target, entry):
    print(f"skip {name} (already present)")
    return
  if target.exists():
    print(f"refresh {name} (incomplete)")
    shutil.rmtree(target)

  target.parent.mkdir(parents=True, exist_ok=True)

  with tempfile.TemporaryDirectory(prefix="riveqt-bootstrap-") as temp_dir:
    clone_dir = Path(temp_dir) / "checkout"
    run(
      [
        "git",
        "init",
        str(clone_dir),
      ]
    )
    run(
      [
        "git",
        "-C",
        str(clone_dir),
        "remote",
        "add",
        "origin",
        entry["url"],
      ]
    )
    run(
      [
        "git",
        "-C",
        str(clone_dir),
        "fetch",
        "--depth",
        "1",
        "origin",
        str(entry["rev"]),
      ]
    )
    run(
      [
        "git",
        "-C",
        str(clone_dir),
        "checkout",
        "--detach",
        "FETCH_HEAD",
      ]
    )
    print(f"copy {clone_dir} -> {target}")
    shutil.copytree(
      clone_dir,
      target,
      ignore=shutil.ignore_patterns(".git"),
    )
  apply_dependency_compatibility(target, entry)
  if not dependency_ready(target, entry):
    raise RuntimeError(f"bootstrap copied {name}, but the expected files are still missing")
  print(f"ready {name}")


def ensure_pip_dependency(entry: dict[str, str], refresh: bool) -> None:
    target = ROOT / entry["path"]
    name = entry["path"]
    if target.exists() and refresh:
        shutil.rmtree(target)

    if target.exists() and any(target.iterdir()):
        print(f"skip {name} (already present)")
        return
    if target.exists():
        shutil.rmtree(target)

    target.mkdir(parents=True, exist_ok=True)
    run(
        [
            sys.executable,
            "-m",
            "pip",
            "install",
            "--no-compile",
            "--target",
            str(target),
            entry["package"],
        ]
    )
    print(f"ready {name}")


def main() -> int:
  parser = argparse.ArgumentParser(
    description="Fetch the third-party sources used by QtQuickRivePlugin2."
  )
  parser.add_argument(
    "--refresh",
    action="store_true",
    help="Delete and re-fetch existing third-party directories.",
  )
  args = parser.parse_args()

  ensure_git_dependency(rive_runtime_entry(), args.refresh)

  rive_cpp_dir = ROOT / RIVE_RUNTIME_PATH
  for entry in gather_rive_dependencies(rive_cpp_dir):
    ensure_git_dependency(entry, args.refresh)

  for entry in PIP_SOURCES:
    ensure_pip_dependency(entry, args.refresh)

  print("third-party bootstrap complete")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
