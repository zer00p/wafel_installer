#!/usr/bin/env python3
"""
Build script to compile Guide/*.md into Guide/html/*.html
styled to match wafel.xyz.

Usage:
    python3 build_guide.py

Requires: python3 (no external dependencies)
"""

import os
import re
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GUIDE_DIR = os.path.join(SCRIPT_DIR, "Guide")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "Guide_html")

# Ordered list of pages: (md_filename, display_title)
PAGES = [
    ("START_HERE.md", "Start Here"),
    ("WafelInstaller.md", "Wafel Installer"),
    ("SaveBackup.md", "Backups"),
    ("DumpInstallGames.md", "Dump & Install Games"),
]


# ---------------------------------------------------------------------------
#  Minimal Markdown-to-HTML converter (no external dependencies)
# ---------------------------------------------------------------------------

def md_to_html(md_text):
    """Convert Markdown text to HTML. Handles the subset used in the Guide."""
    lines = md_text.split("\n")
    html_parts = []
    in_code_block = False
    code_block_lines = []
    in_list = None  # "ul" or "ol"
    list_indent_stack = []  # stack of (indent_level, list_type)

    i = 0
    while i < len(lines):
        line = lines[i]

        # --- Fenced code blocks ---
        if line.strip().startswith("```"):
            if not in_code_block:
                # Close any open list
                html_parts.extend(_close_all_lists(list_indent_stack))
                in_code_block = True
                code_block_lines = []
            else:
                code = "\n".join(code_block_lines)
                code = _escape_html(code)
                html_parts.append(f"<pre><code>{code}</code></pre>")
                in_code_block = False
            i += 1
            continue

        if in_code_block:
            code_block_lines.append(line)
            i += 1
            continue

        # --- Blank line ---
        if line.strip() == "":
            # Close lists on blank line (only if next non-blank line isn't a list item)
            j = i + 1
            while j < len(lines) and lines[j].strip() == "":
                j += 1
            if j < len(lines):
                next_line = lines[j]
                if not re.match(r'^(\s*)([-*]|\d+\.)\s', next_line):
                    html_parts.extend(_close_all_lists(list_indent_stack))
            else:
                html_parts.extend(_close_all_lists(list_indent_stack))
            i += 1
            continue

        # --- Headings ---
        heading_match = re.match(r'^(#{1,6})\s+(.*)', line)
        if heading_match:
            html_parts.extend(_close_all_lists(list_indent_stack))
            level = len(heading_match.group(1))
            text = _inline_format(heading_match.group(2).strip())
            anchor = _make_anchor(heading_match.group(2).strip())
            html_parts.append(f'<h{level} id="{anchor}">{text}</h{level}>')
            i += 1
            continue

        # --- Horizontal rule ---
        if re.match(r'^---+\s*$', line.strip()):
            html_parts.extend(_close_all_lists(list_indent_stack))
            html_parts.append("<hr>")
            i += 1
            continue

        # --- Unordered list ---
        ul_match = re.match(r'^(\s*)([-*])\s+(.*)', line)
        if ul_match:
            indent = len(ul_match.group(1))
            text = _inline_format(ul_match.group(3))
            _handle_list_item(html_parts, list_indent_stack, indent, "ul", text)
            i += 1
            continue

        # --- Ordered list ---
        ol_match = re.match(r'^(\s*)(\d+)\.\s+(.*)', line)
        if ol_match:
            indent = len(ol_match.group(1))
            text = _inline_format(ol_match.group(3))
            _handle_list_item(html_parts, list_indent_stack, indent, "ol", text)
            i += 1
            continue

        # --- Regular paragraph ---
        html_parts.extend(_close_all_lists(list_indent_stack))
        para_lines = [line]
        i += 1
        while i < len(lines):
            next_line = lines[i]
            if (next_line.strip() == "" or
                next_line.strip().startswith("```") or
                re.match(r'^#{1,6}\s+', next_line) or
                re.match(r'^(\s*)([-*]|\d+\.)\s', next_line) or
                re.match(r'^---+\s*$', next_line.strip())):
                break
            para_lines.append(next_line)
            i += 1

        para_text = " ".join(l.strip() for l in para_lines)
        para_text = _inline_format(para_text)
        html_parts.append(f"<p>{para_text}</p>")

    # Close any remaining open lists
    html_parts.extend(_close_all_lists(list_indent_stack))

    return "\n".join(html_parts)


def _handle_list_item(html_parts, stack, indent, list_type, text):
    """Manage nested list opening/closing."""
    if not stack:
        html_parts.append(f"<{list_type}>")
        stack.append((indent, list_type))
    elif indent > stack[-1][0]:
        html_parts.append(f"<{list_type}>")
        stack.append((indent, list_type))
    else:
        while len(stack) > 1 and indent < stack[-1][0]:
            _, lt = stack.pop()
            html_parts.append(f"</li></{lt}>")
        if stack and stack[-1][1] != list_type:
            _, lt = stack.pop()
            html_parts.append(f"</li></{lt}>")
            html_parts.append(f"<{list_type}>")
            stack.append((indent, list_type))
        else:
            html_parts.append("</li>")
    html_parts.append(f"<li>{text}")


def _close_all_lists(stack):
    """Close all open list elements."""
    parts = []
    while stack:
        _, lt = stack.pop()
        parts.append(f"</li></{lt}>")
    return parts


def _escape_html(text):
    """Escape HTML special characters."""
    return (text
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace('"', "&quot;"))


def _inline_format(text):
    """Apply inline Markdown formatting: bold, italic, code, links."""
    # Escape HTML entities in regular text but NOT in already-processed parts
    # We need to be careful about ordering here

    # First, protect code spans (backtick)
    code_spans = {}
    counter = [0]

    def replace_code(m):
        key = f"\x00CODE{counter[0]}\x00"
        counter[0] += 1
        code_spans[key] = f'<code>{_escape_html(m.group(1))}</code>'
        return key

    text = re.sub(r'`([^`]+)`', replace_code, text)

    # Escape HTML in remaining text (but not our placeholders)
    parts = re.split(r'(\x00CODE\d+\x00)', text)
    escaped_parts = []
    for part in parts:
        if part in code_spans:
            escaped_parts.append(part)
        else:
            escaped_parts.append(_escape_html(part))
    text = "".join(escaped_parts)

    # Links: [text](url)
    def replace_link(m):
        link_text = m.group(1)
        url = m.group(2)
        # Rewrite .md links to .html
        if url.endswith('.md'):
            url = url[:-3] + '.html'
        return f'<a href="{url}">{link_text}</a>'

    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', replace_link, text)

    # Bold + Italic: ***text*** or ___text___
    text = re.sub(r'\*\*\*(.+?)\*\*\*', r'<strong><em>\1</em></strong>', text)
    text = re.sub(r'___(.+?)___', r'<strong><em>\1</em></strong>', text)

    # Bold: **text** or __text__
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'__(.+?)__', r'<strong>\1</strong>', text)

    # Italic: *text* or _text_ (but not inside words for underscore)
    text = re.sub(r'\*(.+?)\*', r'<em>\1</em>', text)
    text = re.sub(r'(?<!\w)_(.+?)_(?!\w)', r'<em>\1</em>', text)

    # Restore code spans
    for key, val in code_spans.items():
        text = text.replace(key, val)

    return text


def _make_anchor(text):
    """Generate an anchor ID from heading text."""
    text = re.sub(r'[^\w\s-]', '', text)
    text = text.strip().lower()
    text = re.sub(r'[\s]+', '-', text)
    return text


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

    for idx, (md_file, title) in enumerate(PAGES):
        md_path = os.path.join(GUIDE_DIR, md_file)
        html_file = md_file.replace(".md", ".html")
        html_path = os.path.join(OUTPUT_DIR, html_file)

        print(f"  {md_file} -> Guide_html/{html_file}")

        with open(md_path, "r", encoding="utf-8") as f:
            md_text = f.read()

        content = md_to_html(md_text)

        nav = build_nav(PAGES, idx)
        nav_bottom = build_nav_bottom(PAGES, idx)

        html = HTML_TEMPLATE.format(
            title=title,
            nav_links=nav,
            nav_links_bottom=nav_bottom,
            content=content,
        )

        with open(html_path, "w", encoding="utf-8") as f:
            f.write(html)

    print(f"\nDone! {len(PAGES)} pages written to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
