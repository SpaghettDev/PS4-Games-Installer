# Work tracker

- [x] Remove Unity files and initialize independent repository.
- [x] Root Makefile, application identity, assets, and revision packaging.
- [x] Library metadata, installed discovery, settings persistence, and offline cache.
- [x] Tile/list dashboard, dialogs, controller navigation, splash, and About.
- [x] Artwork loading, background animation, desktop preview, and screenshots.
- [x] Restore the user's preferred r17 bubble style; retain animation toggle and motion checks.
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
- [x] Save both library selections before game launch and restore by title/URL after restart.
- [x] Add confirmed Installed > Uninstall through the native application service.
- [x] Store local Title ID ratings, installation history and launch request statistics separately from catalogs.
- [x] Show star ratings, Installed / Was installed badges and library-specific footer counts.
- [x] Replace the SDK's empty statvfs output with a native filesystem-capacity query.
- [x] Move HTTP transfer buffers to the heap and give native background workers explicit 512 KiB stacks.
- [x] Correct BGFT's system-visible staging path and submit validated PKG identity/size.
- [x] Reuse complete packages despite the SDK stat size mismatch.
- [x] Keep registration/task errors visible; distinguish downloading from installing.
- [x] Add local Title ID categories, labels, descriptions and tags, with drafts and explicit save results.
- [x] Create/rename/delete local categories/tags and browse/filter/group by them.
- [x] Show category/label on both layouts and selected local description previews.
- [x] Remote artwork downloads work (user report for r16, 2026-10-16).
- [x] Application confirmed working on real PS4 11.00 + GoldHEN (user report, 2026-10-16).
- [ ] Complete the individual console acceptance checks below.

## Console acceptance checks

- [ ] Retest r21: use PS4 IME for category, label, tags and description from both libraries.
- [ ] Retest r21: save, refresh/restart and uninstall; personal metadata stays by Title ID.
- [ ] Retest r21: browse/filter/group local categories and tags, including names with no assigned games.
- [ ] Retest r21: rename/delete a category/tag; assignments update, notes/ratings/history remain.
- [ ] Retest r21: cancel an edit and retry a failed save without losing the draft.
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
- [ ] Retest r19: restored r17 bubbles flow upward in Tiles and Summary;
  Settings > Background animation pauses/resumes them without affecting artwork.
- [x] Manipulator navigation works perfectly (user confirmation for r17, 2026-10-16).
- [ ] Circle closes each window and Circle twice exits; Options opens its menu.
- [ ] O cancels PS4 keyboard URL edits without changing the saved source.
- [x] Companion source loads real metadata (44 packages in r16 log) and remote
  artwork (user confirmation, 2026-10-16).
- [ ] Internal and connected external titles appear with correct local metadata/sizes.
- [x] X launches games correctly in r17 (user confirmation, 2026-10-16).
- [x] r17 lists all installed games (user confirmation, 2026-10-16).
- [ ] Retest r19: reopen after launching a game; same library/title/search restored.
- [ ] Retest r19: rate 1–5 stars, refresh/restart and confirm ratings remain by Title ID.
- [ ] Retest r19: cancel/confirm Uninstall, then verify Was installed, rating and history survive restart.
- [ ] Retest r19: Available footer shows the installable package count when switching libraries.
- [ ] A package installs through BGFT and its task survives closing the application.
- [x] r19 direct download reaches completion on hardware without the reported crash
  in the observed Back to Bed attempts (user report and FTP log, 2026-10-16).
- [ ] Retest r20: Back to Bed registers, installs and appears on the PS4 menu.
  r19 BGFT rejected it with 0x80990086; no matching saved task was found.
- [ ] Retest r20: Retry reuses the complete staged file, and a persistent dialog
  shows an actual registration error if the system still refuses the request.
- [ ] Storage panel detects a PS4 Extended Storage drive and updates after disconnect.
- [ ] Install to each PS4 system destination and verify the resulting game location.
- [ ] Retest r17: X in Installation destination changes the selected PS4 default;
  disconnect Extended Storage and verify the unavailable drive cannot be selected.
- [ ] Continue large-package/repeated-transfer stability checks after r19's stack repair;
  no r17 crash dump was available to establish its exact fault.
- [ ] Retest r19: internal/external capacities are correct, including disconnect and full/unknown states.
- [ ] Clear failed r16 downloads once, then install through the direct fallback,
  observe progress, retry an error, and verify staging cleanup.
- [ ] Returning/restarting refreshes installed state and reconciles tracked tasks.
- [ ] Offline startup retains Installed and the cached remote library.
- [ ] Missing/corrupt artwork, unavailable storage and connection failures recover.
