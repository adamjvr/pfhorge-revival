#!/usr/bin/env python3
"""Build a carefully attributed, best-available Marathon 1 visual upgrade pack.

This does NOT claim to be a 100% complete unified HD replacement. It downloads
current upstream plugin releases from their original Simplici7y pages, preserves
their contents, organizes them into separate folders, and adds attribution and
integrity manifests. Uses only the Python standard library.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import http.cookiejar
import json
import os
import re
import shutil
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from datetime import datetime, timezone
from pathlib import Path


def emit_progress(phase: str, label: str, fraction=None,
                  *, indeterminate: bool = False, **details: object) -> None:
    payload: dict[str, object] = {
        "phase": phase,
        "label": label,
        "indeterminate": indeterminate,
    }
    if fraction is not None:
        payload["fraction"] = max(0.0, min(1.0, float(fraction)))
    payload.update(details)
    print("PFHORGE_PROGRESS " + json.dumps(payload, separators=(",", ":")), flush=True)

CONFIG = {
    "display_name": "Marathon 1 Best-Available HD/3D Visual Pack",
    "game_name": "Marathon (1994)",
    "package_root": "Marathon-1-Best-Available-HD-Visual-Pack",
    "output_zip": "Marathon-1-Best-Available-HD-Visual-Pack.zip",
    "components": [
        {
            "name": "TTEP Updated Plugin (M1) @ 1024x1024",
            "version": "7.0",
            "folder": "01-TTEP-M1-Walls-1024",
            "archive_name": "TTEP-M1-Walls-1024-v7.0.zip",
            "source_page": "https://simplici7y.com/items/ttep-updated-plugin-m1-1024x1024/",
            "download_url": "https://simplici7y.com/items/ttep-updated-plugin-m1-1024x1024/downloads/new",
            "scope": "Environment/wall textures at up to 1024x1024; not monsters, weapons, or scenery.",
            "notes": "Deactivate or remove the lower-resolution built-in TTEP v7 plugin to prevent duplicate/conflicting replacements.",
            "credits": [
                "Tim Vogel — original Total Texture Enhancement Package v7 artwork",
                "Zetren — plugin update for modern Aleph One and 1024x1024 packaging",
                "Bungie — original Marathon artwork and designs"
            ]
        },
        {
            "name": "xBR Monsters for M1",
            "version": "1.3",
            "folder": "02-xBR-Monsters-M1",
            "archive_name": "xBR-Monsters-M1-v1.3.zip",
            "source_page": "https://simplici7y.com/items/xbr-monsters-for-m1/",
            "download_url": "https://simplici7y.com/items/xbr-monsters-for-m1/downloads/new",
            "scope": "Monster and marine sprites at 16x original resolution, including every color palette.",
            "notes": "This is an algorithmic xBR upscale rather than newly painted artwork.",
            "credits": [
                "Flippant Sol — plugin author and packaging",
                "General Tacticus — M1 shapes indexing assistance",
                "Treellama — Aorta workflow assistance",
                "Hawkynt — batch-upscaling program",
                "Hyllian — xBR 4x scaling algorithm",
                "Bungie — original Marathon sprites"
            ]
        },
        {
            "name": "Tacticus' M1 Weapons Redux",
            "version": "1.1",
            "folder": "03-Tacticus-M1-Weapons-Redux",
            "archive_name": "Tacticus-M1-Weapons-Redux-v1.1.zip",
            "source_page": "https://simplici7y.com/items/tacticus-m1-weapons-redux-2/",
            "download_url": "https://simplici7y.com/items/tacticus-m1-weapons-redux-2/downloads/new",
            "scope": "HD weapons, projectiles, and most item graphics.",
            "notes": "The upstream description says most items, not every item; this is one reason the combined pack is not labeled 100% complete.",
            "credits": [
                "General Tacticus — HD replacement artwork, animation, and plugin",
                "Bungie — original Marathon weapons, projectiles, items, and designs"
            ]
        },
        {
            "name": "3D Scenery for M1",
            "version": "1.2",
            "folder": "04-3D-Scenery-M1",
            "archive_name": "3D-Scenery-M1-v1.2.zip",
            "source_page": "https://simplici7y.com/items/3d-scenery-for-m1/",
            "download_url": "https://simplici7y.com/items/3d-scenery-for-m1/downloads/new",
            "scope": "Replaces Marathon 1 two-dimensional scenery sprites with three-dimensional models.",
            "notes": "Use shader rendering and turn bloom off. Upstream acknowledges that some visual errors cannot be entirely avoided.",
            "credits": [
                "General Tacticus — 3D models, textures, and plugin",
                "Bungie — original Marathon scenery and designs"
            ]
        },
        {
            "name": "Marathon over Tau Ceti E-I Landscape Texture",
            "version": "3.0 (8192x3072)",
            "folder": "05-Tau-Ceti-Landscape-8192",
            "archive_name": "Marathon-Tau-Ceti-Landscape-8192-v3.0.zip",
            "source_page": "https://simplici7y.com/items/marathon-over-tau-ceti-e-i-landscape-texture/",
            "download_url": "https://simplici7y.com/items/marathon-over-tau-ceti-e-i-landscape-texture/downloads/new",
            "scope": "8192x3072 wrap-around landscapes for Marathon and Pfhor levels.",
            "notes": "Set landscape replacement quality to Unlimited. The replacement is an artistic astronomy-based reinterpretation, not a literal upscale of the stock sky.",
            "credits": [
                "liacrow — concept, rendering, stitching, post-processing, and plugin",
                "Celestia contributors — astronomy visualization software",
                "Hugin contributors — panorama stitching software",
                "GIMP contributors — image editing software",
                "Bungie — original Marathon setting, imagery, and designs"
            ]
        }
    ]
}
USER_AGENT = "Mozilla/5.0 (compatible; CFP-Pack-Builder/1.0; +https://simplici7y.com/)"
CHUNK = 1024 * 1024


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(CHUNK), b""):
            digest.update(block)
    return digest.hexdigest()


def opener() -> urllib.request.OpenerDirector:
    cookies = http.cookiejar.CookieJar()
    return urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cookies))


def request(url: str) -> urllib.request.Request:
    return urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "*/*"})


def is_probably_binary(response) -> bool:
    disposition = response.headers.get("Content-Disposition", "").lower()
    content_type = response.headers.get_content_type().lower()
    return "attachment" in disposition or content_type not in {
        "text/html", "text/plain", "application/xhtml+xml"
    }


def stream_response(response, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".part")
    with temporary.open("wb") as output:
        while True:
            block = response.read(CHUNK)
            if not block:
                break
            output.write(block)
    temporary.replace(destination)


def drive_id_from(text: str) -> str | None:
    patterns = (
        r"/file/d/([A-Za-z0-9_-]+)",
        r"[?&]id=([A-Za-z0-9_-]+)",
        r'"downloadUrl":"[^"]*?[?&]id\\u003d([A-Za-z0-9_-]+)',
    )
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            return match.group(1)
    return None


def parse_hidden_form(page: str, base_url: str) -> str | None:
    action_match = re.search(r'<form[^>]+action="([^"]+)"', page, re.I)
    if not action_match:
        action_match = re.search(r"<form[^>]+action='([^']+)'", page, re.I)
    if not action_match:
        return None

    action = urllib.parse.urljoin(base_url, html.unescape(action_match.group(1)))
    fields: dict[str, str] = {}
    for tag in re.findall(r"<input\b[^>]*>", page, re.I):
        name = re.search(r'\bname=["\']([^"\']+)', tag, re.I)
        value = re.search(r'\bvalue=["\']([^"\']*)', tag, re.I)
        if name:
            fields[html.unescape(name.group(1))] = html.unescape(value.group(1) if value else "")
    if not fields:
        return None
    separator = "&" if urllib.parse.urlparse(action).query else "?"
    return action + separator + urllib.parse.urlencode(fields)


def download_google_drive(session, file_id: str, destination: Path) -> None:
    urls = [
        f"https://drive.usercontent.google.com/download?id={urllib.parse.quote(file_id)}&export=download&confirm=t",
        f"https://drive.google.com/uc?export=download&id={urllib.parse.quote(file_id)}&confirm=t",
    ]
    last_page = ""
    last_url = ""

    for initial_url in urls:
        current_url = initial_url
        for _ in range(3):
            with session.open(request(current_url), timeout=120) as response:
                last_url = response.geturl()
                if is_probably_binary(response):
                    stream_response(response, destination)
                    if zipfile.is_zipfile(destination):
                        return
                    destination.unlink(missing_ok=True)
                    break
                raw = response.read(2 * 1024 * 1024)
                last_page = raw.decode("utf-8", errors="replace")

            next_url = parse_hidden_form(last_page, last_url)
            if next_url:
                current_url = next_url
                continue

            confirm = re.search(r"confirm=([0-9A-Za-z_-]+)", last_page)
            if confirm:
                current_url = (
                    "https://drive.usercontent.google.com/download?"
                    + urllib.parse.urlencode({
                        "id": file_id,
                        "export": "download",
                        "confirm": confirm.group(1),
                    })
                )
                continue
            break

    diagnostic = destination.with_suffix(".download-error.html")
    if last_page:
        diagnostic.write_text(last_page, encoding="utf-8")
    raise RuntimeError(
        "Google Drive did not return a ZIP archive. The file may be rate-limited or require "
        f"manual browser confirmation. Diagnostic: {diagnostic.name}"
    )


def download_component(component: dict, cache_dir: Path) -> Path:
    destination = cache_dir / component["archive_name"]
    if destination.exists() and zipfile.is_zipfile(destination):
        print(f"[cached] {component['name']}: {destination.name}")
        return destination

    destination.unlink(missing_ok=True)
    session = opener()
    print(f"[fetch]  {component['name']} {component['version']}")
    try:
        with session.open(request(component["download_url"]), timeout=120) as response:
            final_url = response.geturl()
            if is_probably_binary(response):
                stream_response(response, destination)
            else:
                page = response.read(2 * 1024 * 1024).decode("utf-8", errors="replace")
                file_id = drive_id_from(final_url) or drive_id_from(page)
                if not file_id:
                    direct = parse_hidden_form(page, final_url)
                    if direct:
                        with session.open(request(direct), timeout=120) as direct_response:
                            stream_response(direct_response, destination)
                    else:
                        raise RuntimeError(f"Could not locate the archive link after resolving {final_url}")
                else:
                    download_google_drive(session, file_id, destination)
    except (urllib.error.URLError, TimeoutError, RuntimeError) as exc:
        raise RuntimeError(
            f"Automatic download failed for {component['name']}: {exc}\n"
            f"Open this source page in a browser: {component['source_page']}\n"
            f"Download the ZIP manually as: {destination}\n"
            "Then run the builder again; it will reuse the local archive."
        ) from exc

    if not zipfile.is_zipfile(destination):
        raise RuntimeError(
            f"Downloaded data for {component['name']} is not a ZIP: {destination}\n"
            f"Source: {component['source_page']}"
        )
    return destination


def safe_extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with zipfile.ZipFile(archive) as zf:
        for member in zf.infolist():
            normalized = member.filename.replace("\\", "/")
            if normalized.startswith("/") or ".." in Path(normalized).parts:
                raise RuntimeError(f"Unsafe archive path in {archive.name}: {member.filename}")
            target = (destination / normalized).resolve()
            if os.path.commonpath([str(root), str(target)]) != str(root):
                raise RuntimeError(f"Unsafe archive path in {archive.name}: {member.filename}")
        zf.extractall(destination)


def remove_metadata(root: Path) -> None:
    for path in sorted(root.rglob("*"), reverse=True):
        if path.name in {".DS_Store", "Thumbs.db"}:
            path.unlink(missing_ok=True)
        elif path.is_dir() and path.name == "__MACOSX":
            shutil.rmtree(path, ignore_errors=True)


def flatten_single_root(extracted: Path, destination: Path) -> None:
    remove_metadata(extracted)
    entries = [p for p in extracted.iterdir() if p.name != "__MACOSX"]
    source = entries[0] if len(entries) == 1 and entries[0].is_dir() else extracted
    destination.mkdir(parents=True, exist_ok=True)
    for child in source.iterdir():
        target = destination / child.name
        if child.is_dir():
            shutil.copytree(child, target, dirs_exist_ok=True)
        else:
            shutil.copy2(child, target)



def component_credits(component: dict, archive_sha: str) -> str:
    credits = "\n".join(f"- {name}" for name in component["credits"])
    return f"""# Credits and attribution — {component['name']}

- **Upstream version:** {component['version']}
- **Source page:** {component['source_page']}
- **Retrieved through:** {component['download_url']}
- **Downloaded archive SHA-256:** `{archive_sha}`
- **Replacement scope:** {component['scope']}

## Credited creators and contributors

{credits}

## Important notes

{component['notes']}

## Rights and redistribution

This builder does not assign a new license to upstream artwork. Original notices,
readme files, and licenses contained in the downloaded archive remain in force.
The component was downloaded from its original public release page and reorganized
locally; this Markdown file was added for provenance. Marathon names, game artwork,
and underlying intellectual property are associated with their respective
rightsholders. No endorsement is implied.
"""


def build_readme() -> str:
    component_lines = "\n".join(
        f"- `{c['folder']}` — {c['name']} {c['version']}\n  - {c['scope']}"
        for c in CONFIG["components"]
    )
    return f"""# {CONFIG['display_name']}

## Read this first

**This is not a verified 100% complete unified HD texture pack.** Marathon 1 has
no Community/Freeverse-style release that cleanly replaces every wall, weapon,
item, monster, scenery object, player sprite, projectile, and landscape in one
consistent finished art set.

This package instead combines the strongest available current plugins by visual
category while keeping every upstream release separate and fully attributed.

## Contents

{component_lines}

## Installation

1. Extract this ZIP.
2. Copy the five folders inside `Plugins/` into the `Plugins` folder beside the
   current native Marathon data files used by Aleph One / Classic Marathon.
3. In the plugin or environment preferences, enable the replacements you want.
4. Disable the lower-resolution built-in `TTEP v7` plugin before enabling the
   included 1024x1024 TTEP folder.
5. For `3D Scenery for M1`, use shader rendering and turn bloom off.
6. For the Tau Ceti landscape, set **Replacement Texture Quality → Landscapes**
   to **Unlimited**.
7. Keep the plugin folders decompressed on disk if load times are poor.

## Why it is labeled “best available”

- TTEP covers environment textures, not every visual category.
- Tacticus' weapons pack explicitly covers **most**, not all, items.
- The 3D scenery package notes that some rendering errors remain unavoidable.
- The five components use different techniques and art styles.
- The Tau Ceti sky is a high-resolution reinterpretation rather than a faithful
  pixel-for-pixel upscale.

## Integrity and provenance

Every component folder contains `CREDITS.md`. `SOURCES.md` records the source
pages and downloaded archive hashes. `MANIFEST.json` records hashes for all files
in the generated package. Upstream image/model data is not recompressed or edited.

## Rights

No collective license is asserted for this bundle. Each upstream component retains
its original notices and terms. This builder merely downloads public releases and
organizes a local personal-use copy with additional attribution documentation.
"""


def build_sources(records: list[dict]) -> str:
    rows = [
        "| Component | Version | Scope | Source page | Download endpoint | SHA-256 |",
        "|---|---:|---|---|---|---|",
    ]
    for record in records:
        c = record["component"]
        rows.append(
            f"| {c['name']} | {c['version']} | {c['scope']} | {c['source_page']} | "
            f"{c['download_url']} | `{record['archive_sha256']}` |"
        )
    return """# Upstream sources

The archives were retrieved from the original Simplici7y release endpoints. No
claim is made that these separate works share one license.

""" + "\n".join(rows) + f"""

- Pack build time: {datetime.now(timezone.utc).isoformat()}
- Builder version: 1.0
"""


def build_attribution() -> str:
    sections = [
        "# Full credits and attribution",
        "",
        "## Original game",
        "",
        "- **Bungie** — Marathon (1994), original artwork, designs, characters, setting, and game intellectual property.",
        "",
        "## Replacement components",
        "",
    ]
    for c in CONFIG["components"]:
        sections.append(f"### {c['name']} {c['version']}")
        sections.append("")
        sections.append(f"**Scope:** {c['scope']}")
        sections.append("")
        sections.extend(f"- {name}" for name in c["credits"])
        sections.append("")
        sections.append(f"**Upstream note:** {c['notes']}")
        sections.append("")
    sections.extend([
        "## Package preparation",
        "",
        "The builder adds folder numbering, Markdown attribution, source records, and SHA-256 manifests. It does not repaint or recompress upstream image/model assets.",
        "",
        "This is an unofficial community-use package and is not endorsed by Bungie, Aleph One, Simplici7y, or the individual plugin authors.",
    ])
    return "\n".join(sections) + "\n"


def file_manifest(root: Path) -> list[dict]:
    records = []
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        records.append({
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256_file(path),
        })
    return records


def zip_tree(source_root: Path, output_zip: Path) -> None:
    temporary = output_zip.with_suffix(output_zip.suffix + ".part")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_STORED, allowZip64=True) as zf:
        for path in sorted(source_root.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(source_root.parent).as_posix())
    temporary.replace(output_zip)


def main() -> int:
    parser = argparse.ArgumentParser(description=f"Build {CONFIG['display_name']} from current upstream releases")
    parser.add_argument("--output", type=Path, default=Path("output"), help="Output directory")
    parser.add_argument("--downloads", type=Path, default=Path("downloads"), help="Archive cache directory")
    parser.add_argument("--keep-work", action="store_true", help="Keep extracted staging directory")
    args = parser.parse_args()

    output_dir = args.output.resolve()
    cache_dir = args.downloads.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)

    emit_progress("resolve", "Resolving enhanced texture sources…", 0.02)

    work_parent = Path.cwd() if args.keep_work else Path(tempfile.mkdtemp(prefix="m1-visual-pack-"))
    package_root = work_parent / CONFIG["package_root"]
    if package_root.exists():
        shutil.rmtree(package_root)
    plugins_root = package_root / "Plugins"
    plugins_root.mkdir(parents=True)

    records: list[dict] = []
    try:
        component_count = max(1, len(CONFIG["components"]))
        for component_index, component in enumerate(CONFIG["components"]):
            base_fraction = 0.05 + (component_index / component_count) * 0.64
            emit_progress(
                "download",
                f"Downloading {component['name']} ({component_index + 1}/{component_count})…",
                base_fraction,
                indeterminate=True,
                item=component_index + 1,
                items=component_count,
            )
            archive = download_component(component, cache_dir)
            emit_progress(
                "verify-source",
                f"Verifying {component['name']}…",
                base_fraction + (0.35 / component_count),
            )
            archive_sha = sha256_file(archive)
            extracted = work_parent / ("extract-" + component["folder"])
            if extracted.exists():
                shutil.rmtree(extracted)
            emit_progress("extract", f"Extracting {component['name']}…", base_fraction + (0.48 / component_count))
            safe_extract(archive, extracted)
            destination = plugins_root / component["folder"]
            flatten_single_root(extracted, destination)
            (destination / "CREDITS.md").write_text(
                component_credits(component, archive_sha), encoding="utf-8"
            )
            emit_progress("assemble", f"Adding {component['name']} to the profile…", base_fraction + (0.62 / component_count))
            records.append({
                "component": component,
                "archive": archive.name,
                "archive_sha256": archive_sha,
            })

        emit_progress("document", "Writing credits, rights notices, and manifests…", 0.74)
        (package_root / "README.md").write_text(build_readme(), encoding="utf-8")
        (package_root / "ATTRIBUTION.md").write_text(build_attribution(), encoding="utf-8")
        (package_root / "SOURCES.md").write_text(build_sources(records), encoding="utf-8")
        (package_root / "RIGHTS-NOTICE.md").write_text(
            "# Rights notice\n\nNo collective license is asserted for the upstream artwork and models. "
            "Each component retains its original notices and terms. This local builder "
            "adds only organizational and attribution files. Do not redistribute the "
            "generated combined archive without first checking every upstream component's terms.\n",
            encoding="utf-8",
        )

        manifest = {
            "package": CONFIG["display_name"],
            "game": CONFIG["game_name"],
            "completeness_claim": "Best available; not verified 100% complete",
            "built_at_utc": datetime.now(timezone.utc).isoformat(),
            "builder_version": "1.0",
            "upstream_archives": records,
        }
        manifest_path = package_root / "MANIFEST.json"
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        manifest["packaged_files"] = [
            record for record in file_manifest(package_root)
            if record["path"] != "MANIFEST.json"
        ]
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

        final_zip = output_dir / CONFIG["output_zip"]
        final_zip.unlink(missing_ok=True)
        emit_progress("package", "Packaging enhanced texture profile…", 0.90)
        print(f"[pack]   {final_zip}")
        zip_tree(package_root, final_zip)
        emit_progress("verify", "Verifying generated texture profile…", 0.97)
        print(f"[done]   {final_zip}")
        print(f"[sha256] {sha256_file(final_zip)}")
        emit_progress("complete", "Enhanced texture profile built successfully", 1.0)
        return 0
    finally:
        if not args.keep_work and work_parent.exists():
            shutil.rmtree(work_parent, ignore_errors=True)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Cancelled.", file=sys.stderr)
        raise SystemExit(130)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
