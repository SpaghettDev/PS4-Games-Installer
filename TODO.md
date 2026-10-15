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
- [x] Import the packed LncUtil launcher library from SystemService; validate the generated import table.
- [x] Resolve the entire HTTP client through one handle, including init and cleanup.
- [x] Left-stick/D-pad navigation with deadzone, repeat, dialog suppression and SDL hotplug tests.
- [x] Include patched (`gp`) game metadata in Installed discovery.
- [x] Add Internal / External USB selection using the PS4 Application Install
  Location preference, with connected-drive and registry readback checks.
- [x] Probe PKG URLs and stream a direct-download fallback for servers without
  range support; retain staging until BGFT releases the package.
- [x] Clean up failed/completed owned BGFT tasks before retrying; preserve active tasks.
- [x] Remote artwork downloads work (user report for r16, 2026-10-16).
- [x] Application confirmed working on real PS4 11.00 + GoldHEN (user report, 2026-10-16).
- [ ] Complete the individual console acceptance checks below.

## Console acceptance checks

- [x] Initial console launch reaches the splash and readable startup-error screen
  (user report, 2026-10-16; failed opening the icon in the previous build).
- [x] PS4 keyboard opens and correctly accepts source URL input (user confirmation,
  2026-10-16, PS4 11.00 + GoldHEN).
- [x] Saving the source URL/settings works on the console (user confirmation, 2026-10-16).
- [x] Console shows Installed game tiles and names (user report, 2026-10-16;
  icons were missing in the previous package).
- [x] Read-only FTP check confirmed a saved games URL before the data-directory
  correction (2026-10-16).
- [ ] Retest r13: data moves to `/data/PS4-Game-Installer`, settings and caches are
  preserved, and About displays only the current application's identity.
- [ ] Verify the saved source/settings survive app restart and library refresh,
  and the config-file confirmation is displayed.
- [x] r16 FTP log shows 44 packages loaded/refreshed and a successful connection
  test; user confirms remote artwork downloads work (2026-10-16).
- [ ] Retest r10: Installed tiles, summary backgrounds and details show local icons
  for internal/external titles, including offline startup without a server.
- [ ] Retest r9: loading ends without a forced splash delay.
- [x] Reported end-of-app CE-34878-0 resolved (user confirmation, 2026-10-16).
- [ ] Separately verify closing startup/runtime error screens returns to the PS4
  shell without an error report; this path was not specified in the confirmation.
- [ ] Splash, bubbles, controller repeat and both layouts render smoothly on the PS4.
- [ ] Retest r16: left stick and D-pad navigate tiles/list, sidebar, menus, Settings
  and details; verify drift filtering, repeat and returning from the PS4 keyboard.
- [ ] Circle closes each window and Circle twice exits; Options opens its menu.
- [ ] O cancels PS4 keyboard URL edits without changing the saved source.
- [x] Companion source loads real metadata (44 packages in r16 log) and remote
  artwork (user confirmation, 2026-10-16).
- [ ] Internal and connected external titles appear with correct local metadata/sizes.
- [ ] X launches a registered game; service errors remain readable.
- [ ] Retest r17: X launches a game without `Resolve game launcher: 0x80020003`,
  starts the game and permits returning to the app.
- [ ] Retest r17: Installed includes patched GTA V, Minecraft, DOOM and DIRT 5;
  host replay of the actual SFO files recovered 17 game/homebrew candidates.
- [ ] A package installs through BGFT and its task survives closing the application.
- [ ] Storage panel detects a PS4 Extended Storage drive and updates after disconnect.
- [ ] Install to each PS4 system destination and verify the resulting game location.
- [ ] Retest r17: X in Installation destination changes the selected PS4 default;
  disconnect Extended Storage and verify the unavailable drive cannot be selected.
- [ ] Retest r17: clear failed r16 downloads once, then install through the direct
  fallback, observe progress, retry an error, and verify staging cleanup.
- [ ] Returning/restarting refreshes installed state and reconciles tracked tasks.
- [ ] Offline startup retains Installed and the cached remote library.
- [ ] Missing/corrupt artwork, unavailable storage and connection failures recover.
