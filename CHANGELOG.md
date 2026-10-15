# Changelog

## r8 — 2026-10-16

- Changed native termination to request exit through the PS4 system service after
  application resources and worker threads have been cleaned up. Both normal exit
  and closing an error screen use this path instead of returning through libc.
- Restore the original process credentials and sandbox directories before the exit
  request, including recovery after a partially failed privilege setup.
- Added native lifecycle regressions for successful shutdown, startup/restore
  failures and an unsuccessful shell exit request. The reported CE-34878-0 at the
  end of the application still needs a console retest.

## r7 — 2026-10-16

- Fixed the reported startup failure opening `/app0/assets/icon.png`: load bundled
  artwork before homebrew privilege setup changes the process filesystem root.
- Retained the bundled font in memory so later glyph loading does not depend on
  sandbox asset paths after privilege setup.
- Kept the splash visible for at least three seconds from its first presented
  frame, including startup failures and early controller input.
- Added real-application startup regressions that hide the asset directory at the
  privilege boundary and check both successful and failed startup timing.

## r6 — 2026-10-15

- Added a Settings installation-storage panel with internal/PS4 Extended Storage
  availability, free space and manual refresh after USB connection changes.
- Added guidance for the PS4's Application Install Location setting. In-app
  destination selection is still pending verification of the native control API;
  the panel does not change the installation destination.
- Added storage mount checks, controller coverage and connected/disconnected USB
  desktop captures.

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
