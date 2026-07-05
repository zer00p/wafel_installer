# Wafel Installer Rules

These rules dictate how agents should interact with the Wafel Installer project.

## Building the App
- The application is built using `make`. It relies on the DevkitPro and `wut` toolchain.
- Typically, this is run inside a Docker container (e.g., `docker run --rm -v ${PWD}:/project builder:latest /bin/bash -c "make clean && make"`).

## Building the Guide
- The HTML guide is built using a custom Python script.
- To build the guide, run `python3 build_guide.py` in the project root. This requires Python 3 and `pandoc` to be installed.
- Do not edit the files in the `Guide_html` directory directly. Always edit the markdown files in the `Guide` directory and run the build script.

## Web Compatibility for the Guide
- The HTML produced for the guide must work properly on the **Wii U web browser** as well as on current modern web browsers.
- Stick to standard HTML5 compatible with the older NetFront NX / WebKit engines found on the Wii U. Do not use modern JavaScript frameworks or CSS features without fallbacks.

## Keeping the Guide Updated
- Whenever you make changes to the application's UI, features, or workflows, you must ensure that the markdown files in the `Guide` directory are kept up-to-date to reflect those changes.

## Learning from the User
- If the user has to explicitly explain a project-specific concept, rule, or workflow to you, you should proactively append that information to this `AGENTS.md` file so that future agents are aware of it.
