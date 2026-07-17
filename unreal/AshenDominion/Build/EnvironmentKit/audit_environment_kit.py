"""Audits locally acquired production environment assets without redistributing them.

Run with UnrealEditor-Cmd.exe and -ExecutePythonScript. Missing assets are reported but
only fail the process when -EnvironmentKitStrict is present on the Unreal command line.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


SCRIPT_DIR = Path(__file__).resolve().parent
MANIFEST_PATH = SCRIPT_DIR / "environment-kit.json"
REPORT_PATH = SCRIPT_DIR.parent.parent / "Saved" / "EnvironmentKit" / "audit.json"


def _object_path(root: str, package_path: str) -> str:
    asset_name = package_path.rsplit("/", 1)[-1]
    return f"{root}/{package_path}.{asset_name}"


def _texture_paths(root: str, prefix: str) -> list[str]:
    return [_object_path(root, f"{prefix}{suffix}") for suffix in ("_BC", "_N", "_ORM")]


def _audit() -> dict[str, object]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    root = manifest["content_root"].rstrip("/")
    missing_meshes: list[str] = []
    missing_textures: list[str] = []
    present_meshes: list[str] = []
    present_textures: list[str] = []

    for entry in manifest["priority_meshes"]:
        path = _object_path(root, entry["path"])
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            present_meshes.append(path)
        else:
            missing_meshes.append(path)

    for entry in manifest["priority_surfaces"]:
        for path in _texture_paths(root, entry["prefix"]):
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                present_textures.append(path)
            else:
                missing_textures.append(path)

    return {
        "kit_id": manifest["kit_id"],
        "content_root": root,
        "present_meshes": present_meshes,
        "missing_meshes": missing_meshes,
        "present_textures": present_textures,
        "missing_textures": missing_textures,
        "complete": not missing_meshes and not missing_textures,
    }


def main() -> None:
    report = _audit()
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")

    unreal.log(
        "Vowfall environment kit: "
        f"{len(report['present_meshes'])} meshes and "
        f"{len(report['present_textures'])} textures present"
    )
    for path in report["missing_meshes"]:
        unreal.log_warning(f"Missing environment mesh: {path}")
    for path in report["missing_textures"]:
        unreal.log_warning(f"Missing environment texture: {path}")

    strict = "-EnvironmentKitStrict" in unreal.SystemLibrary.get_command_line()
    if strict and not report["complete"]:
        raise RuntimeError("The production environment kit is incomplete; see audit.json")


main()
