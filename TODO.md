# Work tracker

- [x] Remove Unity files and initialize independent repository.
- [x] Root Makefile, application identity, assets, and revision packaging.
- [x] Library metadata, installed discovery, settings persistence, and offline cache.
- [x] Tile/list dashboard, dialogs, controller navigation, splash, and About.
- [x] Artwork loading, background animation, desktop preview, and screenshots.
- [x] Console URL keyboard, game launching, and installation monitoring.
- [x] Host tests, PS4 package build/validation, and consumer documentation.
- [x] Private `.env` FTP target configuration and `make deploy` with local FTP tests.
- [x] Settings installation-storage availability/free-space panel and manual refresh.
- [x] Fix startup asset loading across privilege setup; show the splash only while loading.
- [x] Route native shutdown through the PS4 shell after cleanup and credential restoration.
- [x] Draw PS4 controller button symbols next to action labels.
- [x] Request X confirm / O cancel for native dialogs in package metadata.
- [x] Resolve installed icons independently from title metadata, including Extended Storage.
- [x] Visible config-save confirmation/errors, draft snapshots and restart persistence tests.
- [x] Bind native HTTP policies to the same imported service initialized for downloads.
- [x] Canonical `/data/PS4-Game-Installer` storage and removal of inherited branding/imports.
- [ ] In-app Internal / External USB destination selection: verify a native setter
  or per-task destination API first. Current installations follow the PS4 system
  Application Install Location setting; a BGFT application slot is not assumed to
  identify a physical drive.
- [ ] Hardware validation on PS4 11.00 + GoldHEN (requires a console).

## Console acceptance checks

- [x] Initial console launch reaches the splash and readable startup-error screen
  (user report, 2026-10-16; failed opening the icon in the previous build).
- [x] Console reaches the source editor and opens the PS4 keyboard (user report,
  2026-10-16; O selects characters in the previous package).
- [x] Console shows Installed game tiles and names (user report, 2026-10-16;
  icons were missing in the previous package).
- [x] Read-only FTP check confirmed a saved games URL before the data-directory
  correction (2026-10-16).
- [ ] Retest r13: data moves to `/data/PS4-Game-Installer`, settings and caches are
  preserved, and About displays only the current application's identity.
- [ ] Retest r12: Save settings shows the config-file confirmation, survives app
  restart, and a valid `/games` source loads without the HTTP pre-init error.
- [ ] Retest r10: Installed tiles, summary backgrounds and details show local icons
  for internal/external titles, including offline startup without a server.
- [ ] Retest r9: loading ends without a forced splash delay; the native keyboard
  selects characters/Done with X and cancels with O.
- [ ] Retest the reported end-of-app CE-34878-0: normal Circle exit and closing a
  startup/runtime error screen return to the PS4 shell without an error report.
- [ ] Splash, bubbles, controller repeat and both layouts render smoothly on the PS4.
- [ ] Circle closes each window and Circle twice exits; Options opens its menu.
- [ ] PS4 keyboard accepts/cancels URL edits and settings persist after restarting.
- [ ] Companion server `/games` loads real metadata and covers on the console.
- [ ] Internal and connected external titles appear with correct local metadata/sizes.
- [ ] X launches a registered game; service errors remain readable.
- [ ] A package installs through BGFT and its task survives closing the application.
- [ ] Storage panel detects a PS4 Extended Storage drive and updates after disconnect.
- [ ] Install to each PS4 system destination and verify the resulting game location.
- [ ] Returning/restarting refreshes installed state and reconciles tracked tasks.
- [ ] Offline startup retains Installed and the cached remote library.
- [ ] Missing/corrupt artwork, unavailable storage and connection failures recover.
