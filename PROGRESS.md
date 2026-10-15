# Implementation progress

## Repository migration

- Removed the old Git history, Unity project, original workflow, and obsolete files.
- Created a new `main` branch with origin `https://github.com/e1z0/PS4-Games-Installer.git`.
- Native code lives in `src/`; third-party notices and GPLv2 license are retained.
- Build tools run in Docker. The first new package revision will be r2.
- Hardware target: PS4 11.00 + GoldHEN. No console checks have been performed.
