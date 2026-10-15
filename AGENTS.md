# Project workflow

- This is PS4 Games Installer, a native C++17 OpenOrbis application. Keep application
  sources in `src/` and use the root Makefile with Docker-managed build tools.
- Maintain `TODO.md`, `CHANGELOG.md`, and `PROGRESS.md` as work progresses. Record
  actual validation results and remaining limitations, not expected outcomes.
- Create local Git commits at meaningful completed milestones. Push only when asked.
- For application changes, run the relevant host tests and compile/validate the PS4
  package. For UI changes, also generate and inspect desktop screenshots.
- Keep dependencies pinned and retain their licenses. Edit the original SVG when
  changing the application icon, then regenerate the package PNG through Make.
- Let the packaging script manage `revision.json`; commit its successful revision
  record with the corresponding source. Do not increment it for failed builds.
- PS4 11.00 + GoldHEN is the first hardware target. Host tests and package validation
  do not establish that installed discovery, IME, launching, BGFT or frame rate have
  been tested on a console. Keep unchecked hardware work explicit.
