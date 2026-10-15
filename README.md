# PS4 Games Installer

Native OpenOrbis dashboard for a homebrew-enabled PS4, created by e1z0.
Developed alongside [FPKGi-node-server](https://github.com/e1z0/FPKGi-node-server).

Implementation is in progress; see TODO.md and PROGRESS.md for current status.

```sh
make image
make
make test
```

Builds use Docker and produce `dist/PS4Game_installer-rN.pkg`. No Unity or Sony SDK
is required. First hardware target: PS4 11.00 + GoldHEN. Hardware compatibility has
not yet been verified. See LICENSE and THIRD_PARTY.md for attribution.
