"""Publish the already-tested package in Actions; never overwrite a published tag."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = "MyasAlex/CHIP8"


def gh(*args):
    return subprocess.check_output(["gh", *args], text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    info = json.loads((ROOT / "version.json").read_text(encoding="utf-8"))
    tag = info["tag"]
    if os.environ.get("GITHUB_REPOSITORY") != REPO or os.environ.get("GITHUB_REF") != "refs/heads/master":
        raise RuntimeError("Publishing is restricted to this repository's master branch")
    metadata = json.loads((args.assets / "build-info.json").read_text())
    if metadata["commit"] != os.environ["GITHUB_SHA"] or metadata["version"] != info["version"]:
        raise RuntimeError("Package does not belong to this commit/version")
    archive = args.assets / metadata["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != metadata["sha256"]:
        raise RuntimeError("Release asset checksum mismatch")
    notes = ROOT / "docs" / "releases" / (tag + ".md")
    if not notes.is_file():
        raise RuntimeError("Missing release notes")
    releases = json.loads(gh("api", f"repos/{REPO}/releases?per_page=100"))
    existing = next((release for release in releases if release["tag_name"] == tag), None)
    if existing and not existing["draft"]:
        print(f"{tag} is already published; keeping it unchanged.")
        return
    assets = [str(archive), str(args.assets / "SHA256SUMS.txt"), str(args.assets / "build-info.json")]
    if existing:
        if existing["target_commitish"] != os.environ["GITHUB_SHA"]:
            raise RuntimeError("An existing draft belongs to another commit")
        gh("release", "upload", tag, "--repo", REPO, "--clobber", *assets)
    else:
        gh("release", "create", tag, "--repo", REPO, "--target", os.environ["GITHUB_SHA"],
           "--title", info["title"], "--notes-file", str(notes), "--draft", *assets)
    release = json.loads(gh("api", f"repos/{REPO}/releases/tags/{tag}"))
    expected = {Path(path).name: Path(path).stat().st_size for path in assets}
    uploaded = {asset["name"]: asset["size"] for asset in release["assets"]}
    if not release["draft"] or any(uploaded.get(name) != size for name, size in expected.items()):
        raise RuntimeError("Draft asset verification failed; release remains unpublished")
    gh("release", "edit", tag, "--repo", REPO, "--draft=false", "--latest")
    print(f"Published {release['html_url']}")


if __name__ == "__main__":
    main()
