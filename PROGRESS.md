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
- Review changed the launch adapter to dynamic lookup; r14 below corrects the
  containing module after the console exposed a missing-file error.

## Console integration and reliability review

- Added dynamic lookup for launch and installed-title services after homebrew
  privilege setup; the launch module path is corrected in r14 below.
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

## Reported end-of-app CE-34878-0 — 2026-10-16

- User clarified that CE-34878-0 appeared at the end of the application. The error
  code alone does not identify the crashing instruction; no console crash dump is
  available. This is a candidate exit-path correction pending hardware validation.
- Both normal exit and closing the startup error screen previously returned from
  main through the C runtime. Inspected the linked runtime and native PS4 homebrew
  exit implementations. PS4 Cheats Manager and Itemzflow use the system service's
  `sceSystemServiceLoadExec("exit", ...)` request, with homebrew credentials restored.
- Extracted native lifecycle handling from the package installer. Startup saves
  original credentials; termination restores them after all application scopes,
  including worker joins and video/controller destruction, have completed.
- Native exit now loads SystemService and requests shell termination on every
  handled exit path. If the request returns, the main thread waits instead of
  falling through libc `_exit`; a rejected request is logged to stderr and the user
  can use the PS4's Close Application command.
- Added tests against the pinned SDK/libjbc declarations with fake native calls,
  covering successful exit, failed credential reading/preparation/application,
  failed restoration and a rejected exit request. Existing real-main regressions
  also check that normal/error shutdown reaches the exit hook after video cleanup.
- `make test` passed all host suites, including artwork/font ASan/UBSan and thirteen
  Python integration tests. The native lifecycle test exercised all six scenarios;
  real-main success/failure cases both reached termination after video cleanup.
- Built `dist/PS4Game_installer-r8.pkg`; `make validate` passed all package hash and
  signature checks. Verified the linked ELF imports `sceSystemServiceLoadExec`
  and its native finish routine waits after the request without returning to libc.
- Updated the changelog, consumer troubleshooting and console acceptance tracker.
  No UI layout changed. No live deployment or console retest was performed at
  this stage. The user's subsequent exit-crash confirmation is recorded below.

Exit-path references:

- https://github.com/bucanero/PS4CheatsManager/blob/main/source/main.c
- https://github.com/LightningMods/Itemzflow/blob/main/itemzflow/source/sig_handler.cpp
- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/SystemService.h

## Splash, controller hints and PS4 keyboard assignment — 2026-10-16

- Removed the requested splash minimum, including the error-path sleep. The
  dashboard now appears as soon as the initial scan is ready; input during startup
  is still consumed so a held button does not open menus over the splash.
- Replaced written face-button names with drawn Cross, Square, Triangle and Circle
  symbols in keycaps, plus Options and L1/R1 keycaps, beside their action text.
  Dialogs use the same hints and the keyboard guidance describes selecting keys
  and the native Done button rather than suggesting X immediately saves the field.
- User clarified that the PS4 keyboard works, but O selects characters while X
  closes it. Inspection found the generated SFO used ATTRIBUTE=0. The new package
  requests Cross for common dialogs using ATTRIBUTE=2 (without the system-default
  override bit), and includes ATTRIBUTE2=2 as used in current native PS4 packaging.
  The system IME remains in place; no global console button mapping is changed.
- Added a regression running the real metadata staging script and pinned publishing
  tool, then parsing the resulting SFO to check the assignment flags. Startup
  regressions now check that a ready dashboard and startup errors are not held
  behind a three-second timer, while retaining asset-root and cleanup-order checks.
- `make test` passed all host suites (including artwork/font ASan/UBSan) and
  fourteen Python integration tests, including real SFO generation. Successful and
  failed startup both passed the updated no-delay regression checks.
- Built `dist/PS4Game_installer-r9.pkg`; `make validate` passed all package hash and
  signature checks. Extracted the package and verified ATTRIBUTE=2, ATTRIBUTE2=2
  and APP_VER=00.09 in its actual SFO, matching the staged metadata.
- Generated fourteen desktop captures and inspected the Available dashboard,
  Settings and URL editor. Updated the library, summary and storage images used
  in the documentation. No live deployment was performed. The corrected native
  keyboard assignment still needs a PS4 retest.

Native packaging reference:
https://github.com/Mayo1970/ioQuake3-PS4/blob/main/Makefile

## Missing installed-title icons — 2026-10-16

- User reported that Installed game tiles appear without icons. Discovery searched
  for `icon0.png` beside the SFO that supplied each title. This misses the console's
  split layout: private metadata is in `/system_data/priv/appmeta`, while artwork
  is in `/user/appmeta`. The old scan also omitted the public external subtree.
- Confirmed separate internal/external metadata and icon paths in Itemzflow's
  installed-library implementation. Discovery now accepts independent icon roots,
  searches the public internal/external paths first, and retains metadata-directory
  fallback for co-located layouts. Empty, non-file and unreadable candidates are
  skipped; missing artwork does not remove the title from Installed.
- Added an offline regression that discovers titles from private metadata, resolves
  their separate public icons and loads the actual PNGs through ArtworkCache.
  It covers internal/external titles, duplicate metadata, an empty earlier icon,
  public-icon priority, missing artwork and the co-located fallback under ASan/UBSan.
- `make test` passed all host suites, including the installed-icon regression under
  ASan/UBSan, and fourteen Python integration tests. Internal, external and fallback
  icons loaded through the cache without any configured remote artwork URL.
- Built `dist/PS4Game_installer-r10.pkg`; `make validate` passed all package hash and
  signature checks. Generated fourteen desktop captures and inspected Installed
  tiles and the details view using the preview's sample artwork.
- No live deployment or console retest was performed. The reported console symptom
  still needs a retest with r10, including internal/external titles and offline use.

Installed-library reference:
https://github.com/LightningMods/Itemzflow/blob/main/itemzflow/source/ls_dir.cpp

## Reported URL/config save failure — 2026-10-16

- Inspected the configured console's app config and recent app log through FTP,
  read-only. The config already contains a nonempty games URL. Its saved path has
  extra suffix characters and returns HTTP 404; the companion `/games` endpoint
  returns HTTP 200 with 44 entries when checked from Linux. No console file was
  changed and no private target/source addresses or credentials are committed.
- The console log also records `Disable redirects (0x80431001)` before fetching.
  Native HTTP initialization used linked imports, but policy functions came from
  an additional `sceKernelLoadStartModule`/dlsym lookup. That can select separate
  service state. Removed the extra HTTP and SSL loads and bound the verified ABI
  directly to the same imports as initialization. Separate service state is the
  suspected cause of this HTTP error; a PS4 retest is still required.
- Save commands now carry their configuration snapshot rather than selecting a
  draft according to whichever screen is current when the command runs. A successful
  save checks the written settings, logs its destination and leaves Settings open
  with confirmation. Failed saves retain the draft and show an inline error.
- Added guidance distinguishing keyboard acceptance from committing settings,
  plus inline connection status so a failed fetch is visible inside Settings.
- Added tests using the real App/UI/storage pipeline for edit/commit, using the
  saved source, delayed command handling, write failure/retry and restart. Added
  tests of the native HTTP branch against pinned SDK declarations and fake services,
  including rejecting any extra service-module load.
- `make test` passed all host suites, including the new App persistence and native
  HTTP adapter tests, artwork/font ASan/UBSan and fourteen Python integrations.
  The native adapter covers successful streaming, rejecting redirects, size limits,
  cancellation, policy errors, resource cleanup and reuse after a failure.
- Built `dist/PS4Game_installer-r12.pkg`; `make validate` passed every package hash
  and signature check. The linked native HTTP object imports the policy/SSL calls
  directly and no longer references module loading or dlsym.
- Generated fourteen desktop captures and inspected Settings with the inline
  connection result, URL, draft/save notice and controller hints all visible.
  r12 supersedes the intermediate r11 build after adding inline connection status.
- Live activity was limited to read-only FTP and HTTP diagnosis. No console file
  was changed, package deployed, or corrected native HTTP behavior retested on PS4.

Native HTTP ABI references:

- https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/network/http.cpp
- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/Http.h

## Application identity and canonical data directory — 2026-10-16

- Removed inherited branding and unrelated repository references from current
  sources, About, README and packaged credits. Removed the unrelated application
  settings importer and its API argument, rather than hiding or renaming it.
- Console startup now selects `/data/PS4-Game-Installer` after privilege setup.
  The first upgraded launch moves this application's previous data directory as
  a unit when the destination is absent, preserving config, tasks, catalog,
  artwork and logs. An existing destination is never overwritten or merged.
- Added model tests for data preservation, repeated launches, fresh installs,
  destination precedence and rejecting a regular file at the data-directory path.
  Updated App persistence tests to use the simplified settings API.
- Added the canonical storage/identity rule to AGENTS.md. All host suites passed,
  including directory preservation, App persistence, native HTTP, artwork/font
  ASan/UBSan and fourteen Python integrations.
- Built `dist/PS4Game_installer-r13.pkg`; `make validate` passed every package hash
  and signature check. Scanned current tracked files, the native ELF and extracted
  package files: no inherited-name references remain.
- Generated fourteen desktop captures and inspected the updated About screen.
  No package was deployed or live console data moved; startup relocation still
  needs a hardware retest.

## Reported game-launch service failure — 2026-10-16

- User reports `Load game launcher: 0x80020002` when selecting an installed game.
  That diagnostic is emitted before resolving or calling the launch function.
  The adapter used an export-library name as a separate system SPRX filename.
- Verified the module/library mapping against the SystemService export table:
  LncUtil functions belong to `libSceSystemService.sprx`. Updated the lookup to
  that module and kept successful handles cached while allowing failed loads
  to retry. No firmware offsets, replacement system files or new stubs are used.
- Initialization errors are checked, including the documented already-initialized
  result. Missing symbols and a missing foreground user now stop before launch
  with readable errors. Launch parameters and positive app-ID success handling
  remain unchanged.
- Added a test of the real native adapter using pinned SDK declarations and fake
  services: only the actual module exists in the fixture. It covers invalid IDs,
  failed load/retry, missing symbols, initialization/user errors, launch failures,
  repeated launches and zero/positive success results.
- `make test` passed all host suites, including the new native launcher test,
  artwork/font ASan/UBSan and fourteen Python integrations.
- Built `dist/PS4Game_installer-r14.pkg`; `make validate` passed all package hashes
  and signatures. Inspected the native object/ELF: the adapter uses SystemService
  and the obsolete standalone-module path is absent. The source identity audit
  still passes. No UI behavior changed in this revision.
- No live deployment or game launch has been performed; PS4 11.00 + GoldHEN
  retesting remains unchecked.

Launcher references:

- https://github.com/shadps4-emu/shadPS4/blob/main/src/core/libraries/system/systemservice.cpp
- https://github.com/LightningMods/Itemzflow/blob/main/itemzflow/include/defines.h

## Recurring library-refresh HTTP failure — 2026-10-16

- User again reports `games: Disable redirects (0x80431001)` with the last valid
  library retained. This occurs during request setup, before sending the URL.
  The installed revision was not identified in the report. The r12 binding change
  alone has not established successful hardware refresh.
- Inspected the configured target's HTTP service read-only over FTP and checked
  the exported setters' argument layout. Checked r14's native imports: init,
  template creation and setters have the expected names and library IDs. These
  checks do not prove which runtime instance the running app selected or establish
  the exact cause of this console report. No target files were changed.
- Replaced linked HTTP entry points with a function table resolved from a single
  system HTTP module handle. Init, settings, transfers and destruction all use
  that table. Removed `-lSceHttp` so the application cannot mix HTTP stub imports
  with the resolved client. Failed module loads and symbol lookups remain retryable.
- Redirect rejection, platform certificate verification, DNS/connect/send/receive
  timeouts, cancellation and size limits remain enforced. Setup failures are not
  ignored and existing catalog data remains available after failed refresh.
- Updated native HTTP tests to supply functions only through the resolver. Any
  direct HTTP import in the production adapter now fails to link in that test.
  Added failed module load, missing/null function and retry cases alongside the
  existing successful fetch, policy failure, redirect, cancellation and size tests.
- `make test` passed all host suites, including native HTTP binding/retry,
  native launcher, App persistence, artwork/font ASan/UBSan and fourteen Python
  integrations. Verified all seventeen requested HTTP exports exist in the
  configured target's service module.
- Built `dist/PS4Game_installer-r15.pkg`; `make validate` passed every package hash
  and signature check. The native HTTP object and linked ELF contain no linked
  HTTP entry points. The source identity audit still passes; no UI code changed.
- No package was deployed and the corrected client still needs a PS4 11.00 +
  GoldHEN refresh/artwork retest.

HTTP lifecycle reference:
https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/samples/net_http/net_http/main.c

## Controller navigation report — 2026-10-16

- User reports navigation does not work with the controller/manipulator. The
  existing native adapter reads digital buttons only, and the preview ignores
  analog axes. Implemented left-stick navigation while retaining D-pad/buttons.
- Added a shared controller state machine and a native SDK pad-data adapter.
  Native byte axes are centered at 128 and converted to the preview's signed
  axis range. Disconnected/failed reads produce no movement.
- Stick navigation uses activation/release deadzones, dominant-axis selection and
  hysteresis near diagonals. Direction repeat begins after 350 ms, then runs at
  100 ms intervals. D-pad overrides analog input; face/menu buttons do not repeat,
  and each controller sample emits at most one action.
- Closing the keyboard suppresses controls still held until release/centering.
  The preview now owns/closes its controller handle and supports hotplug. Both
  backends route controller input through the same state machine.
- Added tests for native pad samples driving the real tile/summary/sidebar/menu/
  settings/details UI, deadzones/repeat, opposing/simultaneous buttons, disconnect
  recovery and keyboard suppression. The SDL test drives the real preview Video
  adapter with a virtual controller, including detach/reconnect and keyboard exit.
- `make test` passed all host suites, including the new controller suite, native
  HTTP/launcher tests, App persistence, artwork/font ASan/UBSan and fourteen Python
  integrations. The SDL virtual-controller test passed repeat, keyboard return
  and detach/reconnect checks using the real Video adapter.
- `make all` reused the matching `dist/PS4Game_installer-r16.pkg`; `make validate`
  passed all hashes and signatures. The native ELF contains the shared controller
  poll/suppression implementation. The source identity audit passes.
- Generated fourteen desktop captures and inspected Installed tiles and Options.
  No live deployment or hardware navigation success is claimed; PS4 11.00 +
  GoldHEN retesting remains unchecked.

Controller references:

- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/_types/pad.h
- https://wiki.libsdl.org/SDL2/SDL_GameControllerGetAxis

## Exit-crash hardware confirmation — 2026-10-16

- User explicitly confirmed that the exit crash is fixed. Marked the reported
  end-of-app CE-34878-0 issue resolved and updated consumer troubleshooting and
  the changelog to reflect this console evidence.
- The exact installed revision and exit path were not stated. The separate
  startup/runtime error-screen shutdown check remains open; no other console
  acceptance checks were marked complete from this confirmation.
- Documentation-only change: reviewed the diff and ran `git diff --check`.
  No application code, package or revision record changed; no rebuild was needed.
