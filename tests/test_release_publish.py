"""Regression checks for draft publication and verified-asset gating."""
from pathlib import Path
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("publisher", Path(__file__).resolve().parents[1] / "scripts/publish_release.py")
publisher = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(publisher)
COMMIT = "b" * 40
PREVIOUS = "a" * 40


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.assets = self.root / "dist"
        self.assets.mkdir()
        (self.root / "docs/releases").mkdir(parents=True)
        info = {"version": "2.1.0", "tag": "v2.1.0", "title": "Studio 2.1"}
        (self.root / "version.json").write_text(json.dumps(info))
        (self.root / "docs/releases/v2.1.0.md").write_text("Release notes")
        archive = self.assets / "CHIP8-Studio-Windows-x64.zip"
        archive.write_bytes(b"verified package fixture")
        metadata = {**info, "commit": COMMIT, "archive": archive.name,
                    "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}
        (self.assets / "build-info.json").write_text(json.dumps(metadata))
        (self.assets / "SHA256SUMS.txt").write_text(metadata["sha256"] + "  " + archive.name)
        self.release = None
        self.writes = []
        self.bad_digest = False

    def draft(self, target=COMMIT):
        return {"id": 91, "tag_name": "v2.1.0", "draft": True, "name": "Studio 2.1",
                "author": {"login": "github-actions[bot]"}, "target_commitish": target,
                "assets": [], "upload_url": "https://uploads.github.com/test{?name}",
                "html_url": "https://github.com/MyasAlex/CHIP8/releases/tag/v2.1.0"}

    def api(self, endpoint, method="GET", payload=None):
        if "/releases/tags/" in endpoint:
            raise AssertionError("A draft must be accessed by ID, not by its missing Git tag")
        if method != "GET":
            self.writes.append((method, endpoint, copy.deepcopy(payload)))
        if endpoint.endswith("/releases?per_page=100"):
            return [copy.deepcopy(self.release)] if self.release else []
        if "/compare/" in endpoint:
            return {"status": "ahead", "merge_base_commit": {"sha": PREVIOUS}}
        if method == "POST":
            self.release = self.draft()
            return copy.deepcopy(self.release)
        if method == "DELETE":
            asset_id = int(endpoint.rsplit("/", 1)[1])
            self.release["assets"] = [a for a in self.release["assets"] if a["id"] != asset_id]
            return None
        if method == "PATCH":
            self.release.update(payload)
            self.release["tag_name"] = payload.get("tag_name", "untagged-test")
        return copy.deepcopy(self.release)

    def upload(self, url, path):
        self.writes.append(("UPLOAD", path.name, None))
        digest = "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()
        if self.bad_digest and path.suffix == ".zip":
            digest = "sha256:" + "0" * 64
        self.release["assets"].append({"id": 100 + len(self.release["assets"]),
                                     "name": path.name, "size": path.stat().st_size,
                                     "digest": digest, "state": "uploaded"})

    def run_publisher(self):
        with patch.object(publisher, "ROOT", self.root), \
             patch.object(publisher, "api_json", side_effect=self.api), \
             patch.object(publisher, "upload_file", side_effect=self.upload), \
             patch.dict(os.environ, {"GITHUB_REPOSITORY": publisher.REPO,
                                     "GITHUB_REF": "refs/heads/master", "GITHUB_SHA": COMMIT}), \
             patch.object(sys, "argv", ["publish_release.py", "--assets", str(self.assets)]), \
             contextlib.redirect_stdout(io.StringIO()):
            publisher.main()

    def test_new_draft_without_git_tag_is_published_by_id(self):
        self.run_publisher()
        self.assertFalse(self.release["draft"])
        self.assertEqual(len(self.release["assets"]), 3)
        self.assertEqual(self.writes[-1], ("PATCH", f"repos/{publisher.REPO}/releases/91",
                                          {"tag_name": "v2.1.0", "target_commitish": COMMIT,
                                           "draft": False, "prerelease": False, "make_latest": "true"}))

    def test_temporary_published_tag_is_corrected_without_replacing_files(self):
        self.release = self.draft(PREVIOUS)
        for path in self.assets.iterdir():
            self.upload(self.release["upload_url"], path)
        self.release["draft"] = False
        self.release["tag_name"] = "untagged-test"
        original_assets = copy.deepcopy(self.release["assets"])
        self.writes.clear()
        self.run_publisher()
        self.assertEqual(self.release["tag_name"], "v2.1.0")
        self.assertEqual(self.release["target_commitish"], PREVIOUS)
        self.assertEqual(self.release["assets"], original_assets)
        self.assertEqual(len(self.writes), 1)

    def test_workflow_draft_from_previous_commit_is_resumed(self):
        self.release = self.draft(PREVIOUS)
        self.release["assets"] = [{"id": 1, "name": "CHIP8-Studio-Windows-x64.zip"}]
        self.run_publisher()
        self.assertEqual(self.release["id"], 91)
        self.assertEqual(self.release["target_commitish"], COMMIT)
        self.assertFalse(self.release["draft"])
        self.assertFalse(any(method == "POST" for method, _, _ in self.writes))
        self.assertTrue(any(method == "DELETE" for method, _, _ in self.writes))

    def test_bad_uploaded_digest_keeps_release_unpublished(self):
        self.bad_digest = True
        with self.assertRaisesRegex(RuntimeError, "asset verification failed"):
            self.run_publisher()
        self.assertTrue(self.release["draft"])
        self.assertFalse(any(payload and payload.get("draft") is False for _, _, payload in self.writes))

    def test_published_release_is_kept_unchanged(self):
        self.release = self.draft()
        self.release["draft"] = False
        self.run_publisher()
        self.assertEqual(self.writes, [])

    def test_manual_draft_is_kept_unchanged(self):
        self.release = self.draft()
        self.release["author"]["login"] = "MyasAlex"
        with self.assertRaisesRegex(RuntimeError, "not created by this release workflow"):
            self.run_publisher()
        self.assertEqual(self.writes, [])


if __name__ == "__main__":
    unittest.main()
