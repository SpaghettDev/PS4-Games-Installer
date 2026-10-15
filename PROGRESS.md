# Implementation progress

## Repository migration

- Removed the old Git history, Unity project, original workflow, and obsolete files.
- Created a new `main` branch with origin `https://github.com/e1z0/PS4-Games-Installer.git`.
- Native code lives in `src/`; third-party notices and GPLv2 license are retained.
- Build tools run in Docker. The first new package revision will be r2.
- Hardware target: PS4 11.00 + GoldHEN. No console checks have been performed.

## Data and platform layer

- Added bounded SFO parsing and internal/external installed-title discovery with registration checks.
- Added settings migration, atomic saves, complete catalog metadata, offline serialization, grouping and matching.
- Added PS4 launch service integration, UTF-16 keyboard support and 64-bit BGFT task progress.
- Host catalog/HTTP, settings, library and SFO fixture tests pass.
- The shared dashboard and desktop renderer are now being compiled and visually checked.

## Dashboard and preview

- Added a shared state machine for menus, settings, dialogs, source editing and Circle exit.
- Added tile and summary layouts, all requested grouping choices, details and About screens.
- Added animated bubble background, cover loading/caching, and an original app icon.
- Desktop preview generated 12 screenshots; tiles, summary and settings were visually inspected.
- Controller flow tests pass. PS4 executable and initial package r2 compile and validate.
- Review found a launch-library stub mismatch; the adapter now resolves the real LncUtil module.
