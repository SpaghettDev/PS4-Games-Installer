# Changelog

## Console confirmation — 2026-10-16

- User confirmed the application works on real PS4 11.00 + GoldHEN hardware.
- User confirmed source URL entry through the PS4 keyboard and saving settings
  both work on the console.
- User confirmed the reported end-of-app CE-34878-0 crash is fixed. The exact
  installed revision and exit path were not specified. Documentation/tracker
  update only; no package revision increment.

## r16 — 2026-10-16

- Added left analog stick navigation to the PS4 app and desktop preview for games,
  sidebar, menus, settings and details. Previously only the D-pad was read.
- Filter stick drift with separate activation/release thresholds, stabilize
  diagonal movement and repeat held directions after a short delay. D-pad input
  takes priority; action buttons still emit once per press.
- Suppress held controls after closing the keyboard until buttons are released
  and the stick is centered. Handle controller disconnect/reconnect in the preview.
- Added native pad-sample, navigation-state and SDL virtual-controller coverage.

## r15 — 2026-10-16

- Use one explicitly resolved HTTP module for initialization, templates, request
  settings, transfers and cleanup, addressing the recurring pre-request
  `Disable redirects (0x80431001)` report. Removed the linked HTTP stub dependency.
- Keep redirect rejection, certificate verification, timeouts and download limits
  enabled. Errors from request setup still stop the transfer and preserve cache.
- Native tests now provide HTTP functions only through the module resolver, so
  accidentally mixing linked HTTP calls into the adapter fails to link. Added
  missing-module/symbol, null-function and retry coverage.

## r14 — 2026-10-16

- Fixed the reported `Load game launcher: 0x80020002` failure by resolving LncUtil
  exports from `libSceSystemService.sprx`, their actual containing module.
- Check launcher initialization errors, accept its already-initialized result,
  reject a missing foreground user and retain retry after a failed module load.
- Added native-adapter tests for the module lookup, launch parameters, positive
  app IDs, failed calls and retry. A successful PS4 launch still needs retesting.

## r13 — 2026-10-16

- Standardized console storage on `/data/PS4-Game-Installer` for settings, catalog,
  tasks, artwork and logs. This app's existing data is moved there on first launch
  when the destination does not exist.
- Removed the unrelated application importer and inherited branding/repository
  references from application sources, About, README and packaged credits.
- Added data-directory tests for preservation, repeated launches, destination
  precedence, fresh installs and blocked paths.

## r12 — 2026-10-16

- Kept Settings open after saving, with a visible config-file confirmation, inline
  connection results and write errors. Accepting the keyboard is explicitly labeled
  as a draft; failed writes preserve the draft for retry.
- Save commands capture the selected settings when pressed. Acknowledgement follows
  writing and checking the resulting `config.json`; successful saves are logged.
- Corrected native HTTP/SSL bindings to use the initialized imports rather than
  loading extra service modules for redirect/time-out and SSL cleanup functions.
  This addresses the console-reported `Disable redirects (0x80431001)` failure.
- Added real App/UI persistence tests covering restart and failed-save retry, and
  native HTTP adapter tests for initialization, policies, streaming and cleanup.

## r10 — 2026-10-16

- Fixed installed-title icon lookup: read public `/user/appmeta` artwork separately
  from `/system_data/priv/appmeta` title metadata, including both external subtrees.
- Keep a fallback for layouts with icons beside their metadata, skip empty or
  unreadable icon files, and retain titles whose artwork is missing.
- Added an offline discovery-to-image-loading regression for internal and external
  titles, duplicate metadata, missing artwork and co-located icon fallback.

## r9 — 2026-10-16

- Removed the artificial three-second splash minimum. Startup shows the dashboard
  as soon as the initial library scan finishes, and errors appear without a delay.
- Replaced written controller button names with drawn PS4 button symbols and
  keycaps alongside action labels throughout the dashboard and dialogs.
- Corrected package metadata to request X confirm / O cancel for the native PS4
  keyboard, matching application controls. Previously O selected characters and
  X closed the keyboard. The system keyboard is retained.
- Updated keyboard guidance and added a regression that generates and reads real
  SFO metadata to check the common-dialog button assignment.

## r8 — 2026-10-16

- Changed native termination to request exit through the PS4 system service after
  application resources and worker threads have been cleaned up. Both normal exit
  and closing an error screen use this path instead of returning through libc.
- Restore the original process credentials and sandbox directories before the exit
  request, including recovery after a partially failed privilege setup.
- Added native lifecycle regressions for successful shutdown, startup/restore
  failures and an unsuccessful shell exit request. The user subsequently confirmed
  the reported end-of-app CE-34878-0 crash is fixed (2026-10-16).

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
