# Work tracker

- [x] Remove Unity files and initialize independent repository.
- [x] Root Makefile, application identity, assets, and revision packaging.
- [x] Library metadata, installed discovery, settings persistence, and offline cache.
- [x] Tile/list dashboard, dialogs, controller navigation, splash, and About.
- [x] Artwork loading, background animation, desktop preview, and screenshots.
- [x] Console URL keyboard, game launching, and installation monitoring.
- [x] Host tests, PS4 package build/validation, and consumer documentation.
- [ ] Hardware validation on PS4 11.00 + GoldHEN (requires a console).

## Console acceptance checks

- [ ] Splash, bubbles, controller repeat and both layouts render smoothly on the PS4.
- [ ] Circle closes each window and Circle twice exits; Options opens its menu.
- [ ] PS4 keyboard accepts/cancels URL edits and settings persist after restarting.
- [ ] Companion server `/games` loads real metadata and covers on the console.
- [ ] Internal and connected external titles appear with correct local metadata/sizes.
- [ ] X launches a registered game; service errors remain readable.
- [ ] A package installs through BGFT and its task survives closing the application.
- [ ] Returning/restarting refreshes installed state and reconciles tracked tasks.
- [ ] Offline startup retains Installed and the cached remote library.
- [ ] Missing/corrupt artwork, unavailable storage and connection failures recover.
