#!/usr/bin/env python3
"""
Build script to compile Guide/*.md into Guide_html/*.html
styled to match wafel.xyz.

Uses pandoc for Markdown-to-HTML conversion.

Usage:
    python3 build_guide.py

Requires: python3, pandoc
"""

import os
import re
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GUIDE_DIR = os.path.join(SCRIPT_DIR, "Guide")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "Guide_html")

# Ordered list of pages paths: [(md_filename, display_title), ...]
PATHS = [
    [
        ("index.md", "Home"),
        ("GettingStarted.md", "Getting Started"),
        ("WafelInstaller.md", "Wafel Installer"),
        ("SaveBackup.md", "Backups"),
        ("DumpInstallGames.md", "Dump & Install Games"),
    ],
    [
        ("index.md", "Home"),
        ("Uninstall.md", "Uninstall"),
    ]
]


def pandoc_convert(md_path):
    """Convert a Markdown file to an HTML fragment using pandoc."""
    result = subprocess.run(
        ["pandoc", md_path, "-f", "markdown", "-t", "html5", "--no-highlight"],
        capture_output=True, text=True
    )

    if result.returncode != 0:
        print(f"  ERROR: pandoc failed for {md_path}:", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)

    return result.stdout


def rewrite_md_links(html):
    """Rewrite .md links to .html in the generated HTML."""
    return re.sub(
        r'(<a\s+href="[^"]*?)\.md(")',
        r'\1.html\2',
        html
    )


# ---------------------------------------------------------------------------
#  HTML template
# ---------------------------------------------------------------------------

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <title>{title} - Wafel Installer Guide</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link href="guide.css" rel="stylesheet">
</head>
<body>

<div class="guide-container">

<div class="guide-nav">
{nav_links}
</div>

{content}

<div class="guide-nav-bottom">
{nav_links_bottom}
</div>

<div class="guide-footer">
    <div class="guide-contrib">
        Want to improve this guide? <a href="{github_edit_url}">Contribute on GitHub</a>!
    </div>
    <a href="https://wafel.xyz">wafel.xyz</a> &mdash;
    <a href="https://github.com/zer00p/wafel_installer">GitHub</a>
</div>

</div>

</body>
</html>
"""


def build_nav(pages, current_index):
    """Build navigation HTML with links to all pages, highlighting current."""
    links = []
    for idx, (md_file, title) in enumerate(pages):
        html_file = md_file.replace(".md", ".html")
        if idx == current_index:
            links.append(f'<a href="{html_file}" class="nav-current">{title}</a>')
        else:
            links.append(f'<a href="{html_file}">{title}</a>')
    return "\n".join(links)


def build_nav_bottom(pages, current_index):
    """Build prev/next navigation for bottom of page."""
    links = []
    if current_index > 0:
        prev_file = pages[current_index - 1][0].replace(".md", ".html")
        prev_title = pages[current_index - 1][1]
        links.append(f'<a href="{prev_file}">&larr; {prev_title}</a>')
    if current_index < len(pages) - 1:
        next_file = pages[current_index + 1][0].replace(".md", ".html")
        next_title = pages[current_index + 1][1]
        links.append(f'<a href="{next_file}">{next_title} &rarr;</a>')
    return "\n".join(links)


# ---------------------------------------------------------------------------
#  Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Copy guide.css from source directory to output directory
    css_src = os.path.join(GUIDE_DIR, "guide.css")
    css_dst = os.path.join(OUTPUT_DIR, "guide.css")
    if os.path.exists(css_src):
        shutil.copy2(css_src, css_dst)
    else:
        print(f"  Warning: guide.css not found in {GUIDE_DIR}")

    # Copy wafel_installer-icon.png from source directory to output directory
    icon_src = os.path.join(GUIDE_DIR, "wafel_installer-icon.png")
    icon_dst = os.path.join(OUTPUT_DIR, "wafel_installer-icon.png")
    if os.path.exists(icon_src):
        shutil.copy2(icon_src, icon_dst)
    else:
        print(f"  Warning: wafel_installer-icon.png not found in {GUIDE_DIR}")

    generated = set()
    total_pages = 0
    for path in PATHS:
        for idx, (md_file, title) in enumerate(path):
            if md_file in generated:
                continue
            generated.add(md_file)
            total_pages += 1

            md_path = os.path.join(GUIDE_DIR, md_file)
            html_file = md_file.replace(".md", ".html")
            html_path = os.path.join(OUTPUT_DIR, html_file)

            print(f"  {md_file} -> Guide_html/{html_file}")

            content = pandoc_convert(md_path)
            content = rewrite_md_links(content)

            nav = build_nav(path, idx)
            nav_bottom = build_nav_bottom(path, idx)
            github_edit_url = f"https://github.com/zer00p/wafel_installer/blob/master/Guide/{md_file}"

            html = HTML_TEMPLATE.format(
                title=title,
                nav_links=nav,
                nav_links_bottom=nav_bottom,
                content=content,
                github_edit_url=github_edit_url,
            )

            with open(html_path, "w", encoding="utf-8") as f:
                f.write(html)

    print(f"\nDone! {total_pages} pages written to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
