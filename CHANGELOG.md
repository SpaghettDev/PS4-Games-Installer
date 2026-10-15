# Changelog

## r4 — 2026-10-15

- Added `make deploy` to build, verify and upload the current package to the FTP
  target configured in the private `.env` file.
- Added an example target configuration and consumer deployment instructions.
- Excluded private environment files from Docker build contexts as well as Git.
- Added local FTP integration tests for uploads, failed transfers, remote size
  checks, package integrity and configuration parsing without shell evaluation.

## r3 — 2026-10-15

Native PS4 Games Installer redesign (hardware validation pending).

- Replaced the inherited repository with an independent native OpenOrbis project.
- Moved application sources to `src/` and builds to the root Makefile.
- Added new application identity, original SVG icon, and licensed Noto Sans font.
- Added persistent, input-based package revisions and Docker-managed tools.
- Added settings persistence/migration, full catalog details and installed-title matching.
- Added installed metadata discovery, game launching and BGFT progress adapters.
- Added tile and summary views, installed/available libraries, grouping, and controller menus.
- Added graphical source configuration, game details, splash, About and Circle exit confirmation.
- Added animated bubbles, asynchronous cover caching, offline libraries and task tracking.
- Added desktop preview, sample covers, screenshots and controller navigation tests.

- Added transaction tests for failed/concurrent builds and input changes while packaging.
- Added artwork/Unicode/clipping checks under AddressSanitizer and UBSan.
- Added 50,000-entry library navigation coverage and reusable screenshot captures.
- Resolved launcher functions from LncUtil and loaded private installed-title services after privilege setup.
- Bundled dependency notices and added consumer setup, server integration and troubleshooting docs.
- Verified unchanged builds retain their revision; r3 supersedes the intermediate r2 package.
