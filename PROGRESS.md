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

## Console integration and reliability review

- Completed dynamic loading for LncUtil and AppInstUtil after homebrew privilege setup.
- Added UTF-8/UTF-16 keyboard conversion, readable startup failures and task ledger polling.
- Hardened artwork invalidation, settings schema validation and large-library grid caching.
- Package r3 built in Docker and passed all LibOrbisPkg hash/signature checks.
- A second unchanged `make` reused r3 without compiling or consuming a revision.
- Final host tests pass: catalog/HTTP, settings/SFO/installed fixtures, controller flows,
  50,000-entry navigation, Unicode/artwork and renderer bounds with ASan/UBSan.
- Four packaging integration tests pass: unchanged/changed/clean builds, failed builds,
  concurrent builds and changes to inputs during packaging.
- Final desktop captures include splash, both library views, Options, Settings, URL entry,
  details, About, exit confirmation, empty library and offline presentation.
- Hardware behavior remains unverified; console acceptance checks are recorded in TODO.md.

## Final checkout verification

- A fresh local clone of the committed repository built successfully using the Docker image.
- That checkout rebuilt r3, retained the identical input fingerprint/revision record, and
  remained clean in Git after building. Package validation passed in the clean checkout.
- Inspected the packaged file list: executable, font, new icon, platform stubs and license
  notices are present; Unity files and desktop sample covers are absent.
- Final splash, summary and About screenshots were inspected, and selected captures are
  committed under docs/images for consumer documentation.
- All software implementation items are complete. Hardware checks remain explicitly
  unchecked because this workspace has no PS4 test connection.

## FTP deployment

- Added `make deploy` with a private `.env` target; the existing local configuration
  is preserved and remains ignored. Added `.env.example` for new checkouts.
- Excluded environment files from the Docker build context. The deployment parser
  handles quoted values/comments without shell evaluation or credential logging.
- Deployment validates configuration before building, checks the current revision's
  fingerprint and SHA-256, then uploads through passive FTP with a temporary name,
  optional remote size verification and final rename. Failed transfers clean up
  their temporary file when the server remains reachable.
- `make test` passed all host suites and ten Python integration tests (six deployment
  cases and four packaging transactions). The deployment cases also pass with the
  host's older Python, including invalid encoding and stale/corrupt package checks.
- Built `dist/PS4Game_installer-r4.pkg`; `make validate` passed all package hash and
  signature checks. The deployment helper verified the real artifact and parsed the
  existing private target inside Docker without opening a network connection.
- The configured live target has not been contacted; FTP tests use a local socket
  server, including an invocation of the actual Makefile's deploy target.

## Installation storage request (partial implementation)

- Inspected OpenOrbis BGFT/AppInstUtil headers, flatz's published BGFT ABI, current
  homebrew installer sources and ps4libdoc's exported storage symbols. These do not
  establish a verified per-download Internal/External selection mechanism or the
  signature/semantics of a system installation-location setter. No guessed flag,
  application-slot-to-drive mapping, or registry write was added.
- Added a Settings storage information panel showing mounted, writable internal
  storage and PS4 Extended Storage with free space. Stale mount directories on the
  parent filesystem do not count as connected drives. Triangle refreshes the check.
- The panel preserves draft settings when returning with Circle and directs users
  to the PS4's Application Install Location setting for the actual selection.
- The requested in-app destination chooser remains incomplete and is tracked in
  TODO.md. Existing install behavior is retained; the UI does not claim to override
  the destination. No console connection or live deployment has been performed.
- Host tests passed, including stale/missing mount paths, a separate writable
  filesystem, storage refresh commands and preserving draft settings on Circle.
  All ten Python deployment/packaging tests also passed.
- Built and validated `dist/PS4Game_installer-r6.pkg` with all package hash/signature
  checks passing. r6 supersedes the intermediate r5 storage-panel build after the
  free-space display was corrected for a full drive.
- Generated fourteen desktop captures and visually inspected Settings plus both
  connected/disconnected USB states. Selected storage captures are in docs/images.

Research references:

- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/Bgft.h
- https://github.com/flatz/ps4_stub_lib_maker_v2/blob/master/include/bgft.h
- https://github.com/idc/ps4libdoc/tree/9.00
- https://www.playstation.com/en-ca/support/hardware/ps4-external-hdd-support/

## Reported console startup failure — 2026-10-16

- User hardware report: initial splash rendered briefly, then the application
  displayed `Cannot open /app0/assets/icon.png` on its readable error screen.
- Extracted r6 and confirmed the packaged icon's SHA-256 matches the source. The startup order opened the font
  before privilege setup but opened the icon afterward. The pinned libjbc changes
  the process root/current/jail directory to the global root; `/app0` refers to the
  application's sandbox path before that transition.
- Moved icon loading before privilege setup and retained the bundled font bytes in
  memory for FreeType's lazy glyph reads. The renderer uses loaded artwork afterward.
- The splash minimum is now three seconds measured from its first presented frame,
  including initialization time. Early controller input is consumed without opening
  menus; initialization failures retain the splash until the same deadline.
- Added regressions using the real main/App/UI/renderer with test-only platform and
  video adapters. They remove asset-path access during privilege setup and check
  successful dashboard rendering plus failure-path timing.
- `make test` passed all native host suites (including artwork/font ASan/UBSan)
  and twelve Python integration tests. Both new startup regressions passed:
  the dashboard rendered after asset-path removal, and normal/error startup each
  kept the splash for at least three seconds.
- Built `dist/PS4Game_installer-r7.pkg`; `make validate` passed all package hash
  and signature checks. Generated fourteen desktop captures and visually inspected
  the splash and dashboard with the memory-backed font and preloaded icon.
- The corrected build has not been retested on a PS4 or deployed to the live target.

libjbc reference: https://github.com/bucanero/ps4-libjbc/blob/835fe016ff0ae5dd89b9249f39cc0fe093fd07dd/jailbreak.c
