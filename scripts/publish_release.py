"""Publish verified packages by release ID, including drafts without a Git tag."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REPO = "MyasAlex/CHIP8"


def api_json(endpoint, method="GET", payload=None):
    command = ["gh", "api", "--method", method, endpoint]
    temporary = None
    try:
        if payload is not None:
            with tempfile.NamedTemporaryFile(mode="w", suffix=".json", encoding="utf-8", delete=False) as file:
                json.dump(payload, file, ensure_ascii=False)
                temporary = Path(file.name)
            command += ["--input", str(temporary)]
        result = subprocess.check_output(command, text=True)
        return json.loads(result) if result.strip() else None
    finally:
        if temporary:
            temporary.unlink(missing_ok=True)


def upload_file(upload_url, path):
    endpoint = upload_url.split("{", 1)[0]
    if urllib.parse.urlsplit(endpoint).hostname != "uploads.github.com":
        raise RuntimeError("Unexpected release upload host")
    request = urllib.request.Request(
        endpoint + "?name=" + urllib.parse.quote(path.name, safe=""),
        data=path.read_bytes(),
        headers={
            "Authorization": "Bearer " + os.environ["GH_TOKEN"],
            "Accept": "application/vnd.github+json",
            "Content-Type": "application/octet-stream",
        },
        method="POST")
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    info = json.loads((ROOT / "version.json").read_text(encoding="utf-8"))
    tag = info["tag"]
    if os.environ.get("GITHUB_REPOSITORY") != REPO or os.environ.get("GITHUB_REF") != "refs/heads/master":
        raise RuntimeError("Publishing is restricted to this repository's master branch")
    commit = os.environ["GITHUB_SHA"]
    metadata = json.loads((args.assets / "build-info.json").read_text())
    if (metadata["commit"], metadata["version"], metadata["tag"]) != (commit, info["version"], tag):
        raise RuntimeError("Package does not belong to this commit/version")
    archive = args.assets / metadata["archive"]
    if hashlib.sha256(archive.read_bytes()).hexdigest() != metadata["sha256"]:
        raise RuntimeError("Release asset checksum mismatch")
    notes = ROOT / "docs" / "releases" / (tag + ".md")
    assets = [archive, args.assets / "SHA256SUMS.txt", args.assets / "build-info.json"]
    expected = {path.name: (path.stat().st_size, "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest())
                for path in assets}
    releases = api_json(f"repos/{REPO}/releases?per_page=100")
    existing = next((release for release in releases if release["tag_name"] == tag), None)
    if existing and not existing["draft"]:
        print(f"{tag} is already published; keeping it unchanged.")
        return
    if existing:
        if existing["author"]["login"] != "github-actions[bot]" or existing["name"] != info["title"]:
            raise RuntimeError("Existing draft was not created by this release workflow")
        previous = existing["target_commitish"]
        if previous != commit:
            if not re.fullmatch(r"[0-9a-f]{40}", previous):
                raise RuntimeError("Existing draft has an unexpected target")
            comparison = api_json(f"repos/{REPO}/compare/{previous}...{commit}")
            if comparison["status"] != "ahead" or comparison["merge_base_commit"]["sha"] != previous:
                raise RuntimeError("Existing draft is not an ancestor of this verified build")
        release = api_json(f"repos/{REPO}/releases/{existing['id']}", "PATCH", {
            "target_commitish": commit, "name": info["title"],
            "body": notes.read_text(encoding="utf-8"), "draft": True})
    else:
        release = api_json(f"repos/{REPO}/releases", "POST", {
            "tag_name": tag, "target_commitish": commit, "name": info["title"],
            "body": notes.read_text(encoding="utf-8"), "draft": True, "prerelease": False})
    release_id = release["id"]
    # A draft can have no Git tag yet. Use its ID for every operation.
    for path in assets:
        for old_asset in release["assets"]:
            if old_asset["name"] == path.name:
                api_json(f"repos/{REPO}/releases/assets/{old_asset['id']}", "DELETE")
        upload_file(release["upload_url"], path)
    release = api_json(f"repos/{REPO}/releases/{release_id}")
    uploaded = {asset["name"]: asset for asset in release["assets"]}
    if not release["draft"] or release["target_commitish"] != commit:
        raise RuntimeError("Draft identity/target changed; refusing publication")
    for name, (size, digest) in expected.items():
        asset = uploaded.get(name)
        if not asset or asset["state"] != "uploaded" or asset["size"] != size or asset.get("digest") != digest:
            raise RuntimeError("Draft asset verification failed; release remains unpublished")
    published = api_json(f"repos/{REPO}/releases/{release_id}", "PATCH",
                         {"draft": False, "make_latest": "true"})
    if published["draft"] or published["tag_name"] != tag:
        raise RuntimeError("GitHub did not publish the expected release")
    print(f"Published {published['html_url']}")


if __name__ == "__main__":
    main()
