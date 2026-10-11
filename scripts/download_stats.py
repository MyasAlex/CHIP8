"""Read public GitHub counts for Windows ZIP assets; no third-party packages."""
import json
import urllib.error
import urllib.request

REPO = "MyasAlex/CHIP8"
ASSET = "CHIP8-Studio-Windows-x64.zip"


def main():
    rows = []
    for page in range(1, 101):
        request = urllib.request.Request(
            f"https://api.github.com/repos/{REPO}/releases?per_page=100&page={page}",
            headers={"Accept": "application/vnd.github+json", "User-Agent": "CHIP8-Studio-download-stats"})
        with urllib.request.urlopen(request, timeout=30) as response:
            releases = json.load(response)
        if not releases:
            break
        for release in releases:
            if release["draft"]:
                continue
            downloads = sum(asset["download_count"] for asset in release["assets"] if asset["name"] == ASSET)
            rows.append((release["tag_name"], downloads, release["html_url"]))
    for tag, downloads, url in rows:
        print(f"{tag}: {downloads} Windows ZIP downloads — {url}")
    print(f"Total Windows ZIP downloads: {sum(row[1] for row in rows)}")
    print("Asset downloads are not unique users or installations.")


if __name__ == "__main__":
    try:
        main()
    except (urllib.error.URLError, KeyError, ValueError) as error:
        raise SystemExit(f"Unable to read public release statistics: {error}")
