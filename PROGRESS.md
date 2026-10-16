# Implementation progress

## r28 failed native-task recovery — 2026-10-16

- User reports the same exact-content task 268435469 / 0x80991404 still blocks
  installation with 0x80990086, and requested upstream installation research.
- Reviewed upstream C# direct-download/local-install call sites and the native
  bridge at commit `61b00ce423be9d7bdee3e6951962536269dd0b00`. Both local and web
  installs use the same native BGFT APIs already used here; upstream provides no
  failed-task recovery for 0x80990086. Its local generic-error return precedes the
  special duplicate notice, and other conflict branches uninstall the title.
  Kept installed titles intact rather than adopting that uninstall behavior.
- Added bounded recovery for a refused local registration: actual PKG identity and
  subtype must locate a task whose progress reports an error. Repeat the identity
  lookup and progress check immediately before unregistering; retry registration
  once using the retained file and application slot. Active/unknown tasks and
  remote-only identities cannot trigger cleanup. No console tasks were modified
  during implementation or reference research.
- Added native-service regressions for orphan recovery, both duplicate codes,
  active tasks, lookup/progress failures, identity changes/resumption, cleanup
  errors, repeated conflict, second-registration/start failure, retained new task
  ownership, update subtype/slot and refusal of remote-only cleanup.
- Full Docker `make -j4 test` passed all host/native-service suites and 15 Python
  integrations, including the new recovery cases. FTP deployment tests used only
  their localhost fixture; no package was deployed to the console.
- `make all` built `dist/PS4Game_installer-r28.pkg`; `make validate` passed package
  hashes/signatures and packed imports. Source fingerprint, revision and manifest
  checksum match the final artifact. Source identity and whitespace audits pass.
- Updated consumer recovery instructions, CHANGELOG and TODO. This changes the
  installer backend only; no render/layout changes or new screenshots. PS4
  installation success and the new native recovery path remain unconfirmed.

Reference:
https://github.com/ItsJokerZz/UnityOrbisBridge/blob/61b00ce423be9d7bdee3e6951962536269dd0b00/source/plugin/source/pkg.cpp

## Installation refusal investigated from console logs — 2026-10-16

- Pulled current app log, queue/task journals, staging listing and targeted BGFT
  metadata over the configured private FTP connection, read-only. Evidence is
  ignored under `build/hardware-install-20261016T155942Z/`; no console files,
  tasks or installed titles were changed. Installed app revision is unconfirmed.
- The latest two Back to Bed (`CUSA02537`, catalog version `01.01`) attempts reuse
  the complete 245,760,000-byte staged package and fail during local BGFT
  registration with `0x80990086`. Exact Content ID lookup reports existing task
  `268435469` (`0x1000000D`), whose progress error is `0x80991404`.
- Queue contains one Failed entry without an owned task ID; `tasks.json` is empty.
  Preserve the complete package and do not automatically unregister that task.
  The 40 saved `/user/bgft/task/*/d0.pdb` records contain no matching Title ID;
  numeric suffix `0000000d` belongs to an unrelated disc title. No matching saved
  notification was found. The existing task's original URL/provenance is therefore
  unconfirmed; do not identify it solely by a directory-number suffix.
- Confirmed in source that the requested disk-first flow already exists: packages
  from the current non-Range server are saved under the application's `downloads`
  directory and handed to the native installer using `/user/data/...`. Repeating
  the download will not resolve the reported existing-task conflict.
- Next hardware check: cancel the failed Back to Bed entry through PS4
  Notifications > Downloads if present, then retry the retained local package.
  If no matching entry is visible, close the app, restart the console, enable
  GoldHEN and retry. Successful registration/installation remains unverified.
- Documentation-only update; reviewed evidence and source call flow, and ran
  `git diff --check`. No application changes, package build or revision increment.

## r27 faster browsing and named server profiles — 2026-10-16

- User requested suggestions 4 and 6: page/letter navigation, remembered searches,
  and named server profiles. Queue controls/backups/undo/badges are outside this change.
- Adding L2/R2 page controls, a letter picker, bounded persistent search history,
  profile source-set drafts and isolated caches with stale-fetch protection.

- Implemented page jumps, filtered-view letter counts/picker and 12 recent searches
  with deduplication, explicit clear/cancel and immediate session persistence.
- Added native digital L2/R2 and SDL trigger mapping, plus desktop Page Up/Down.
  Existing controller/IME suppression tests pass with both trigger bindings.
- Added 16 complete-source profiles in Settings, save-copy/rename/delete/use drafts,
  explicit Save/discard and shared local records/queue state. Legacy settings load.
- Added keyed source caches, legacy-cache adoption, immediate source-cache selection
  and rejection of stale catalog results. Old remote enrichment is cleared from
  installed entries when changing sources; owned transfers keep their saved URLs.
- Corrected misleading indentation in two new test loops before the full build.
  Generated 68 captures and inspected Search, Jump to letter, Profiles and page
  navigation. Native compilation/import checks pass. Corrected an invalid URL
  fixture in the new profile tests. The full host suite passed, including all 15
  Python integrations, native adapters, source-switch rollback, offline caches,
  session persistence and stale-fetch rejection.
- Final review added a one-time legacy-cache migration marker and an App regression
  for externally edited source URLs, preventing repeat adoption under a new server.
  The complete suite passed again after this fix, and r26 built/validated.
- Final page-boundary review found the nine-row letter picker skipped eight items.
  Matched its page stride to nine, added a regression and a second-page capture.
  Browsing/App/controller regressions passed after this correction; native compile
  and packed import validation passed. Inspected the corrected second-page capture.
- Built `dist/PS4Game_installer-r27.pkg` (6,619,136 bytes). `make validate` passes
  package signatures/hashes and packed imports; source fingerprint, manifest hash
  and revision match the final artifact. r26 was an intermediate build before the
  picker pagination correction; it was not deployed.
- Regenerated all 69 captures at r27, inspected profile selection, expanded Settings,
  page navigation and both letter pages, and refreshed the consumer images. Added
  README examples/screens for profiles, recent searches and the letter picker;
  all README image links resolve.
- CHANGELOG and TODO record implementation and validation. Identity/whitespace
  audits pass. New PS4 controller/profile acceptance remains unchecked in TODO;
  no console deployment or remote push was made.

## r25 navigation and maintenance improvements — 2026-10-16

- User authorized suggestions 1–6. Implementing sidebar shortcuts, sorting,
  Updates, dependency checks, storage cleanup and USB diagnostics. Package
  checksum verification (suggestion 7) is outside this requested change.
- Cleanup will require evidence that staging files are released; ambiguous legacy
  files and active tasks remain protected. Diagnostic export uses an allowlisted
  report rather than copying configuration, URLs, personal profiles or raw logs.

- Implemented contextual sidebar Downloads/Collections, Updates filtering/version
  labels and persistent sidebar/Updates state. Expanded sort fields/order with old
  setting migration and unknown values last; paginated the expanded Settings.
- Added enqueue/retry dependency guards, persisted queued-base links, independent
  eligible scheduling and a fresh installed scan before update/DLC transfer. Queued
  dependencies fail clearly when their base fails/cancels. Optional catalog base
  version requirements persist; firmware metadata remains advisory.
- Added measured artwork/staging/reclaimable/protected usage and confirmed cleanup.
  Explicit released/in-use lease files protect old/ambiguous packages after crashes;
  failed starts that cannot unregister now retain their owned task IDs.
- Added selected-field USB diagnostics, bounded operation/code summaries, omission
  of raw logs/URLs/personal fields, unique exports and USB readback. Source files and
  existing USB player backups remain independent of diagnostic exports.
- Initial OpenOrbis compile exposed unsupported C++ stream headers; replaced the
  log splitter with bounded string parsing. Host compilation also required a lambda
  directory deleter. Native compilation/import validation then passed.
- Initial feature/App integration tests passed, including missing-base, failed
  dependencies, stale UI vs fresh console state, cleanup and disconnected USB checks.
  Corrected two new test fixture setups before running the complete suite.
- Generated 60 desktop captures and inspected Updates, expanded Settings, blocked
  install requirements, cleanup and USB diagnostics. Adjusted singular file counts
  and removed lease implementation terminology from the consumer cleanup screen.
  The complete suite then passed (all C++ suites and 15 Python integrations),
  including HTTP lease release/protection cases. A second run also passed after
  the successful dependency-ordering test and explicit empty Updates screen.
- Final review fixed queued-package back navigation to Game packages, protected
  Failed state for a retained task whose start failed, and tolerated cache files
  disappearing during measurement. Added retained-task polling/cancellation and
  package back-navigation regressions. The complete suite passed again after these
  fixes, including all 15 Python integrations and the native adapter checks.
- Built `dist/PS4Game_installer-r25.pkg` (6,619,136 bytes). `make validate` passes
  package hashes/signatures and packed import checks; revision, source fingerprint
  and manifest SHA-256 match the final package.
- Regenerated all 61 desktop captures at r25. Inspected sidebar focus, Updates in
  summary/tiles, blocked and queued-base requirements, expanded Settings, cleanup,
  diagnostic export confirmation and empty Updates. Refreshed all consumer images
  and added Updates/cleanup/diagnostic captures; README image links resolve.
- README, CHANGELOG and TODO describe the six requested improvements, controls,
  migration, ownership protection, export omissions and remaining console checks.
  Identity/whitespace audits pass. No console deployment or remote push was made;
  r25 hardware acceptance remains explicitly unchecked in TODO.md.

## r24 background download stage refinement — 2026-10-16

- Recorded whether a PS4 task was registered for a remote background transfer
  or a staged local package. Background bytes below total now show Downloading;
  a local copy remains Installing. Phase changes and transfer mode persist.
- A completed stage cannot regress because a later progress sample lacks its
  completion fields. Added App checks for Downloading → Installing and saved mode.
- Full host suite passed, including all 15 Python integrations, native adapter
  checks and the App Downloading → Installing transition with durable transfer mode.
- Built `dist/PS4Game_installer-r24.pkg`; package hash/signature and packed-import
  validation passed. The source fingerprint and package SHA-256 match the manifest.
- Regenerated all 48 desktop captures, inspected the queue/verification/package
  screens and refreshed the README images to r24. Identity and whitespace audits
  pass. Console acceptance remains unchecked in TODO.md; no deployment or push.

## r23 library and installer improvements — 2026-10-16

- User authorized all seven suggested improvements. Implementing persistent queue
  and installation verification, bulk local organization, USB profile backups,
  saved collections, related package/version views and a source setup helper.
- Retaining the existing native installer and Title ID records; console end-to-end
  installation acceptance will remain unchecked until actual hardware evidence.
- Added `queue.json` with 200 bounded active/history items, restart recovery,
  transfer samples, retry/cancel and explicit stage diagnostics. The transfer and
  BGFT monitor use separate managed workers; native BGFT calls share a mutex.
  Cancellation stops/unregisters only recorded tasks, retains ownership/staging
  on service errors and never treats a failed service query as file release.
- Added fresh-scan installation verification and a two-minute unconfirmed failure.
  Existing installed copies cannot establish success for vanished tasks. Known
  updates require the requested version; DLC completion remains a system report,
  separate from individual add-on discovery and game tiles.
- Added bulk category/add/remove/clear-tag editing with unique Title ID selection,
  atomic all-or-nothing profile updates and retained drafts on failed writes.
- Added up to 64 named rule collections and built-in shortcuts. Personal profile
  schemas 1/2 still load; writes use schema 3. Saved filters retain removed names
  instead of silently broadening, and renames update their rules.
- Added ordinary USB profile export/readback and validated replacement restore,
  with a unique recovery copy of the original raw profile, including corrupt data.
  Game files, source settings, task ownership and queue state are excluded.
- Added base/update/DLC related-package views and Title ID grouping, numeric
  dotted-version comparison, installed/latest-version details and honest unknown
  DLC status. Catalog category remains the installer routing field.
- Added staged source setup with URL/format testing, type counts, missing ID
  warnings, empty-catalog reporting, HTML detection and source-specific errors.
- Full host suite passed, including all 15 Python integrations, native adapters,
  rollback/retry, queue verification/cancellation/restart, source validation,
  old-profile loading, saved rules and USB recovery tests. Native compilation and
  packed launcher import checks passed before final packaging.
- Full host suite also passed after the final USB-connection and stale-legacy-task
  checks. Generated 48 desktop captures; inspected bulk selection/editing, both
  saved-rule pages, queue diagnostics, USB restore confirmation, source results,
  package grouping and expanded Settings/Options. Added three README captures.
- Built `dist/PS4Game_installer-r23.pkg`; `make validate` passes hashes, signatures
  and packed launcher imports. Source fingerprint, manifest SHA-256 and revision
  match. Source identity and whitespace checks pass. No deployment or push.
- Further queue phase labeling refinement is tracked in the next revision.

BGFT cancellation reference:
https://github.com/flatz/ps4_remote_pkg_installer/blob/master/installer.c

## r22 quick category/tag assignment — 2026-10-16

- Added direct Assign category and Assign tags game actions in both libraries,
  reusing the local Title ID profile, name registries and PS4 keyboard.
- Category selection saves with X; tags toggle with X and save together with
  Square. O cancels the quick draft. Successful persistence returns to Actions;
  failed persistence leaves the picker and draft available for retry.
- Pickers focus the existing assignment or newly entered name. Added game name
  and Title ID context, explicit control hints, visible save errors and game
  action pagination so extra actions cannot overlap the controls.
- Full `make test` passes, including all 15 Python integrations and native adapter
  tests. New UI/App checks exercise both libraries, existing/new names, current
  selection focus, cancellation, category clearing, tag toggling, atomic-write
  failures/retries and preservation of notes, labels, ratings and launch history.
- Built `dist/PS4Game_installer-r22.pkg`; `make validate` passed package hashes,
  signatures and packed launcher imports. Package SHA-256, source fingerprint
  and revision 22 agree with the release manifest.
- Generated 32 desktop captures and visually inspected both quick pickers and
  both game action pages. Controls and menu rows fit their panels. Added the
  quick tag picker to the consumer README alongside the documented controls.
- Source identity and whitespace checks pass. New controls still require console
  acceptance; no deployment or Git push has been performed.

## r21 local game organization — 2026-10-16

- Added personal category, label, description and tags to the existing Title ID
  record, separate from catalog data and the package category used for install
  routing. Both Installed and Available receive the same saved personal fields.
- Added local name registries, including empty categories/tags, case-insensitive
  ASCII reuse, rename with assignment updates, and confirmed deletion that clears
  assignments while preserving all other records. Limits: 256 categories and 256
  tags, 16 tags per title, 64-byte names/labels and 2047-byte descriptions.
- Existing player schema 1 records load with default empty metadata. Writes use
  schema 2 so older releases reject and preserve the profile rather than erase
  new fields when updating ratings. Saves still use atomic replacement, validate
  before writing, preserve corrupt files and retain drafts on write failure.
- Added controller/IME editing, category/tag pickers and a local browser with
  creation, rename, delete confirmation, filtering and clear-filter actions.
  Added local-category/tag grouping and session filter/group restoration. Counts
  stay unique when one title appears in multiple tag groups.
- Tiles/summary always show category and label; selected descriptions preview
  above the footer. Details/search include the local fields. The native build
  compiled and launcher import validation passed.
- Full `make test` passes, including all 15 Python integrations. New checks cover
  legacy profiles, refresh isolation, shared Installed/Available records, name
  creation/rename/deletion, notes after uninstall/restart, failed-save draft
  retention, discard/cancel, tag-picker cursor clamping, multiline details,
  combined filters, multi-tag counts, group selection and session persistence.
- Built `dist/PS4Game_installer-r21.pkg`; `make validate` passes hashes, signatures
  and packed launcher imports. Verified the package SHA-256 and source fingerprint
  against the manifest/revision record. No failed build consumed a revision.
- Generated 29 r21 desktop captures and inspected the editor, tag picker,
  category/tag browser and both dashboard layouts. Refreshed README screenshots
  and documented editing, browsing, management, filtering and data limits.
- Source identity and whitespace checks pass. No deployment or Git push; the
  new console IME/controller flows and long descriptions remain explicit hardware
  checks in TODO.md. Existing native install behavior was retained this turn.

## r20 download versus installation — 2026-10-16

- Pulled current app log, saved catalog/config/task/player data, app.db, staging
  listing, PKG header and small BGFT task metadata through the configured FTP
  target. Read-only inspection; private evidence stays in ignored
  `build/hardware-r19/`. No downloads, installed titles or task files were changed.
- Reported title is Back to Bed, CUSA02537. The staging file is 245,760,000 bytes;
  PKG header declares the same size and its Content ID matches that title. The
  app log reaches full direct download, then reports Already queued. Its mapping
  corresponds to registration error `0x80990086`. `tasks.json` is empty and app.db
  has no matching title. This was a download followed by rejection, not a
  successful installation followed by a missing home-screen icon.
- Inspected saved BGFT task records: none contains that Title/Content ID. Do not
  infer which unrelated task to remove from a duplicate error alone; an in-memory
  service conflict remains possible. New diagnostics query the content-specific
  native lookup and preserve the raw rejection code. If no matching task can be
  confirmed, the dialog advises a full PS4 restart/GoldHEN and retry.
- Local registrations used the app's `/data/...` path. Corrected BGFT requests
  to the system-visible `/user/data/...` path, and read actual PKG identity/size
  from the file before registration. Catalog Title ID mismatches and malformed or
  truncated PKGs fail without submitting a task. This correction has compiled
  for PS4 but has not yet established successful installation on the console.
- The pinned SDK's stat layout reads allocation blocks as `st_size`; the retry
  cache check therefore rejected complete packages. Switched package length
  checks to descriptor seeking. Added a regression runner that deliberately
  reproduces the bad stat size, and native package tests including a sparse 5 GiB
  package. Existing complete staging files remain available for registration retry.
- Download status now says installation follows the download. Added persistent
  failure dialogs for registration and BGFT task errors, without interrupting an
  open keyboard/settings edit. Retry returns through install confirmation.
  Acknowledging an unchanged task error prevents repeated modal popups each poll.
- Full `make test` passes, including all 15 Python integrations. New coverage
  checks canonical storage paths, actual Content ID/size submission, truncated
  headers, Title ID mismatches, 64-bit package sizes, duplicate lookup without
  cancellation, cache reuse despite broken stat, refusal/retry, IME deferral and
  acknowledged-error suppression. One earlier host application wait timed out
  during a concurrent build; added call-site diagnostics, and five consecutive
  application runs plus the final full suite passed afterward.
- Built `dist/PS4Game_installer-r20.pkg`; `make validate` passes all hashes,
  signatures and packed launcher imports. Source fingerprint and package SHA-256
  match their records. Generated 23 r20 desktop captures and inspected the new
  failure dialog; refreshed the README dashboard image.
- Current source identity and whitespace audits pass. No deployment, task
  cancellation, game removal, console installation or Git push was performed.
  Hardware registration/installation and home-screen appearance remain pending;
  the new code does not establish that the console's conflict has cleared.

Primary references reviewed:

- [BGFT system path mapping](https://github.com/Xyhlo/SSPI/blob/main/app/PkgInstallPath.cs)
- [SDK stat ABI investigation](https://github.com/orbis-ports/orbis-compat/blob/master/include/orbis_stat.h)
- [Content-specific BGFT task lookup](https://github.com/Xyhlo/SSPI/blob/main/app/PkgInstaller.cs)

## r19 personal library and download investigation — 2026-10-16

- User confirmed r17 game launching, manipulator navigation and all installed
  titles work on PS4 11.00 + GoldHEN. Launching replaces this foreground app;
  position must be persisted before entering the native launcher. The working
  launch parameters and installed-title filtering are retained.
- Configured FTP target returned No route to host. Requested availability for a
  read-only log/dump pull; no new console evidence or console writes this turn.
  The exact download CE-34878-0 fault remains unconfirmed.
- Disassembled the pinned SDK's statvfs wrapper: it invokes statfs on a temporary
  buffer and never populates the caller's result. This explains the zeroed
  capacity display and staging-space failure. Read the stable kernel statfs
  prefix via `_fstatfs`, with oversized backing storage, checked 64-bit arithmetic
  and explicit full/unknown/read-only states. Native ABI/cleanup tests pass.
- Native HTTP used a 64 KiB automatic transfer buffer, consuming the documented
  small default worker stack before system-library call overhead. Moved transfer
  and catalog buffers to the heap; gave App and artwork workers explicit 512 KiB
  stacks. Native stack-attribute/ownership/error tests pass, and compilation now
  limits HTTP adapter frames to 8 KiB. This repairs a concrete hazard without
  claiming the unavailable crash dump proves its origin.
- Added atomic, validated local `player.json` (Title ID ratings/history/launch
  requests) and `session.json` (both library selections, search and sidebar).
  Corrupt player files are preserved; failed position saves prevent launch.
  Catalog refreshes cannot overwrite personal records. History records titles
  observed installed from this version onward, not unknown pre-existing history.
- Added a native uninstall boundary with valid-title, self/system protection,
  registration checks and readable service errors; its native mock tests pass.
  UI requires a separate confirmation and retains local records afterward.
- Added star rendering, Was installed badges and persistent current-library
  footer counts. Restored the exact r17 Canvas bubble implementation as requested.
- `make test` passes the full host suite and all 15 Python integrations, including
  native HTTP/BGFT/launcher/uninstall/storage, explicit worker stacks, sanitized
  native filesystem output, ratings/history isolation, session restore after
  reordering, failed launch-counter rollback, blocked position writes, corrupt
  player-file preservation, controller dialogs and uninstall cancellation.
- Built `dist/PS4Game_installer-r19.pkg`. `make validate` passes every package
  hash/signature and packed launcher import check. Verified the source fingerprint
  and package SHA-256 against the revision/manifest records; no failed build
  consumed a revision.
- Generated 22 desktop captures and inspected the r19 tile/summary libraries,
  rating dialog, Installed actions, uninstall confirmation and storage panel.
  Refreshed the README image. Renderer tests measure 105,741 moving pixels in
  Tiles and 99,764 in Summary with the restored subtle bubbles; paused frames
  remain identical and tile artwork is unchanged.
- FTP retry still returned No route to host. No deployment, console uninstall,
  live installation or Git push was performed. New console acceptance checks,
  especially download stability, real capacities and native uninstall, remain
  explicit in TODO.md. Source identity audit and `git diff --check` pass.

Native ABI/stack references: pinned
[OpenOrbis v0.5.4](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/tree/v0.5.4)
headers and libc disassembly;
[orbis-compat](https://github.com/orbis-ports/orbis-compat) thread-stack research.
Reference material and private evidence remain in ignored `build/` only.

## r18 bubble visibility — 2026-10-16

- Investigated the reported missing dashboard animation. Existing bubbles used
  fill alpha 9/255 and one-pixel rims at 17/255; summary artwork and its dimmer
  further obscured them. The main loop already passes elapsed time each frame.
- Increased visible contrast, added soft highlights/rims and a faster upward
  drift. Cached six reusable bubble sprites to avoid per-pixel shading work each
  frame. Layer particles after the summary artwork/dimmer and before the controls.
- Kept the saved animation preference and the Settings draft preview. Added a
  full-renderer regression check for exposed-area motion/contrast in both layouts,
  unobscured tile artwork and identical paused frames, plus paired captures.
- `make test` passed the full host suite and all 15 Python cases. The new renderer
  check measured 41,256 exposed pixels moving in Tiles and 41,249 in Summary,
  with peak channel contrast 94 in both. Turning animation off produced identical
  frames; tile artwork stayed unchanged and the Settings draft preview paused.
- `make all` built `dist/PS4Game_installer-r18.pkg`; `make validate` passed package
  hashes/signatures and packed launcher imports. Package source fingerprint and
  SHA-256 manifest match. Generated 19 desktop captures, inspected the tile and
  summary motion pairs, and refreshed the README image from the new dashboard.
- Source identity audit and `git diff --check` passed. No console deployment or
  Git push; PS4 visibility/frame rate still require hardware retesting.

## r17 hardware-driven repairs — 2026-10-16

- Pulled r16 logs, saved settings/catalog/task tracking, installed-title SFOs and
  selected system-service export metadata through the configured FTP target.
  Read-only inspection: no console files, settings or downloads were changed.
  Private evidence remains under ignored `build/hardware-r16/`.
- The log reports repeated launcher `0x80020003`, BGFT `0x80991404`, only five
  installed titles, and an informational storage panel. The user separately
  confirmed that artwork downloads now work.
- Twelve hidden games have active `gp` metadata. A host replay of the actual SFO
  files now finds 17 game/homebrew candidates, including GTA V, Minecraft, DOOM
  and DIRT 5. Native registration checks remain in the console discovery path;
  this replay does not establish the final console count.
- Actual SystemService exports place LncUtil in a packed export library. Replaced
  default-library dlsym with linked LncUtil imports and checked the generated
  Orbis ELF's library/module pair for both initialization and launch.
- The live package endpoint serves a valid PKG with HTTP 200 even when a Range
  request asks for four bytes; HEAD probes also stall for most packages. The
  reference server ignores ranges and includes query strings in filesystem
  lookup. The exact BGFT request causing its 404 was not captured, so that
  particular URL transformation is not claimed as proven.
- Added a PKG probe and a streaming direct-download/local-BGFT fallback for
  servers without ranges, using private internal staging. A full 126,222,336-byte
  package from the live server was streamed to PC staging through the production
  host transfer code; length and PKG magic passed. No console task was submitted,
  and the temporary PC package was removed afterward.
- Added a real Internal / External USB selector using Application Install
  Location registry research, with availability checks, recognized-value checks,
  write/readback verification and rollback on mismatch. This changes the system
  default and does not interpret BGFT application slots as physical drives.
- `make test` passes: catalog/model/UI checks, sanitized artwork checks, native
  launcher/storage/BGFT/HTTP adapters, application state/persistence checks,
  controller tests, HTTP integration, and all 15 Python cases. New application
  checks preserve tracking/staging while copying or when unregister fails, allow
  retry after a failed owned task is retired, and release staging after confirmed
  installation. HTTP coverage includes ranges, ignored ranges, cache reuse, 404,
  HTML, cancellation, low space, missing length and truncated transfers.
- `make all` produced `dist/PS4Game_installer-r17.pkg`. `make validate` passed all
  package hashes/signatures and packed launcher import checks; the current source
  fingerprint matches revision 17. `make screenshots` generated 15 captures;
  inspected connected/disconnected storage, install confirmation and exit screens.
- Source identity audit and `git diff --check` pass. Updated README, changelog,
  tracker and attribution. Deployment tests used disposable localhost FTP servers;
  no package was uploaded to the console and no remote Git push was made. The new
  launch, storage selection and BGFT installation paths still need a PS4 retest.

Research references:

- https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/issues/218
- https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/OELFStrangeLibs.go
- https://github.com/ReSo7200/DirectPackageInstaller-Enhanced/blob/DN8/Payload/main_experimental.c

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

## Confirmed hardware environment — 2026-10-16

- User confirmed the application works on real PS4 11.00 + GoldHEN hardware.
  Updated the README and tracker from a planned hardware target to confirmed
  operation, alongside the previously confirmed exit-crash resolution.
- The installed revision and individual feature results were not specified.
  Detailed acceptance checks remain tracked separately rather than inferring
  launch, installation or other feature results from this general confirmation.
- Documentation-only update; `git diff --check` passed. No application changes,
  rebuild or package revision increment.

## Keyboard and settings-save confirmation — 2026-10-16

- User confirmed the PS4 keyboard now works correctly for entering the source URL
  and confirmed saving also works. Recorded both checks as complete for the
  previously confirmed PS4 11.00 + GoldHEN environment, and updated README and
  changelog status.
- Removed the stale pending keyboard-entry check. Cancellation, saved settings
  after restart/refresh and the separate catalog-download check remain tracked
  independently; those outcomes were not specified in this confirmation.
- Documentation-only update; `git diff --check` passed. No rebuild or package
  revision increment was needed.
