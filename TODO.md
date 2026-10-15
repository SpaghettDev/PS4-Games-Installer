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
- [x] Fix startup asset loading across privilege setup and enforce a three-second splash.
- [ ] In-app Internal / External USB destination selection: verify a native setter
  or per-task destination API first. Current installations follow the PS4 system
  Application Install Location setting; a BGFT application slot is not assumed to
  identify a physical drive.
- [ ] Hardware validation on PS4 11.00 + GoldHEN (requires a console).

## Console acceptance checks

- [x] Initial console launch reaches the splash and readable startup-error screen
  (user report, 2026-10-16; failed opening the icon in the previous build).
- [ ] Retest r7: startup reaches the dashboard with its icon, and splash lasts at least three seconds.
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
