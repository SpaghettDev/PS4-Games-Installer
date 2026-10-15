# PS4 Games Installer

A native PS4 library, game launcher and remote package installer built with the
OpenOrbis toolchain. Created by **e1z0 — (c) 2026 Vintage2000.org**.

[Project](https://github.com/e1z0/PS4-Games-Installer)

**First hardware target: PS4 11.00 with GoldHEN.** The application builds on Linux
and its package validates. User console runs reached the source editor, system
keyboard and Installed listing. The r9 keyboard assignment and r10 installed-icon
corrections need retesting; shell exit, game launching and installation also need validation.
It requires an active homebrew environment; this app does not install GoldHEN.

## What it does

- **Installed:** discover registered games and homebrew on the console, show their
  local icons and metadata, and launch them with X. This works without a server.
- **Available:** browse the complete remote catalog, with installed and queued
  badges. X opens an installation confirmation; the PS4 downloads and installs
  the package in the background after you confirm.
- Switch between a cover tile grid and a summary list. The summary background
  follows the selected game's artwork. Group by category, region, first letter,
  or none; sort by name or size and optionally hide installed remote titles.
- Edit catalog URLs with the PS4 on-screen keyboard, test connectivity, and save
  settings from the app. No FTP configuration is needed for normal setup.
- View detailed metadata, search the current library, and use an offline catalog
  and bounded artwork cache. The blue background has optional floating bubbles.

![Desktop preview with sample artwork](docs/images/library.png)

The preview uses original sample artwork and fictitious IDs/sizes. On the console,
Installed reads local game icons; Available uses your server's `cover_url` images.
Installed artwork is read from `/user/appmeta/<TITLE_ID>/icon0.png` or
`/user/appmeta/external/<TITLE_ID>/icon0.png`, independently of the private title
metadata directory. It does not require a configured server or Internet access.

## Install and set up

1. Build the package below, or obtain a package from this project's releases when
   one is published. The filename is `PS4Game_installer-rN.pkg`.
2. Activate GoldHEN and install the package with your existing package installer.
3. Launch **PS4 Games Installer**, press **Options**, and choose **Settings**.
4. Select **Games JSON URL**, enter the catalog URL, and confirm the keyboard.
5. Choose **Test connection**, then **Save settings**. The library refreshes after
   a source change. Other sources can be added under **Other catalog sources**.

Accepting the keyboard updates a draft. **Save settings** writes
`/data/PS4-Game-Installer/config.json` and keeps Settings open with a visible
confirmation. Connection results and save errors appear inside the Settings window.
If saving fails, the edited draft is kept for another attempt.

The title ID is `PGIN00001`.
Sizes use decimal GB/MB. Installed size and remote package size are separate fields;
unknown values are shown as unknown.

## Using a companion catalog server

Use a companion server that scans your packages, extracts PS4 icons, and serves
catalogs, artwork and package files. Follow your server's Linux/Docker setup
instructions.

Use the JSON link from its catalog page. For example, with its JSON library server
on port 3000:

```text
http://192.168.1.10:3000/games
```

That server's `/games` endpoint serves `games.json`; entering `/games.json` on that
port would request `games.json.json`. A different web server can use any direct JSON
URL, including one ending in `.json`.

The JSON server and package/artwork server may use different ports. The addresses
inside the catalog must also be reachable from the PS4. Advertise your server's LAN
IP or DNS name rather than `localhost`, `127.0.0.1`, or `0.0.0.0`.

The existing catalog format is supported:

```json
{
  "DATA": {
    "http://192.168.1.10:8080/packages/example.pkg": {
      "name": "Example homebrew",
      "title_id": "BREW00000",
      "region": "EUR",
      "version": "1.00",
      "release": "10-15-2026",
      "size": "32000000000",
      "min_fw": null,
      "cover_url": "http://192.168.1.10:8080/covers/example.png"
    }
  }
}
```

A name and direct HTTP(S) package URL are required. Metadata may be missing or null;
size accepts integer bytes or a decimal string. Title IDs enable installed-status
matching. Details also retain additional string metadata supplied by the server.

Serve catalogs as direct HTTP 200, uncompressed JSON responses. Redirects and
interactive authentication are not supported. Package hosting must support direct
URLs, range requests and correct content lengths for BGFT. TLS verification remains
enabled; firmware TLS/certificate support can limit which HTTPS hosts work.

## Controls

| Button | Action |
| --- | --- |
| D-pad | Navigate games, sidebar, menus and details |
| Left at the first tile/list column | Focus the sidebar |
| X / Right on the sidebar | Return to games |
| L1 / R1 | Switch Installed / Available |
| X on Installed | Launch the game |
| X on Available | Install confirmation, or actions for an installed title |
| Triangle | Game actions, including launch/install, details and artwork refresh |
| Square | Game details; Up/Down scroll |
| Options | Refresh, search, view/group/sort/filter, Settings, About and Exit |
| O | Close a window or go back |
| O on the main dashboard | Open exit confirmation; press O again to exit, X to stay |

Selecting Exit from Options opens the same Circle confirmation. Settings are saved
only when Save settings is selected; Circle offers to discard unsaved edits.
Quick view/group/sort/filter changes in Options are saved immediately.
The loading splash is visible while startup runs, with no artificial delay.
The PS4 keyboard uses X to select a character or its Done button, and O to cancel.

## Installation storage

Open **Options > Settings > Install location** to check internal storage and a
connected USB drive configured as **PS4 Extended Storage**, including free space.
Press Triangle to check again after connecting or disconnecting the drive; Circle
returns to Settings and preserves unsaved edits.

The actual destination is currently selected in **PS4 Settings > Storage > Options
> Application Install Location**. Choose System Storage or Extended Storage there
before queuing packages. The app's storage panel is informational: an in-app drive
selector has not yet been implemented because a verified destination-changing API
is still needed. It does not override the PS4 setting or format drives.

A plain exFAT/FAT32 USB package source is not a PS4 Extended Storage installation
destination. See [PlayStation's Extended Storage instructions](https://www.playstation.com/en-ca/support/hardware/ps4-external-hdd-support/)
for configuration and compatibility. Console testing of external installations is
still pending.

## Downloads and offline use

Installation is handed to PS4 BGFT. Closing this application does not cancel a
submitted task. Use **PS4 Notifications > Downloads** for authoritative task status,
completion and cancellation. The app retains task IDs between launches and polls
progress while running. A completed transfer may still be installing.

Existing-title conflicts are reported without automatically uninstalling a game.
This version does not offer uninstall, forced replacement, or download-to-USB modes.
An installed title may require its external drive or original disc to launch.
If an earlier revision reports `Load game launcher: 0x80020002`, install r14 or
later. The launcher adapter now resolves the service from its correct system
module; launching still needs a console retest.

A failed refresh keeps the last valid library. Installed discovery is independent
of the network; cached remote metadata and covers remain available offline. Limits:
8 MiB per catalog/download, 50,000 combined entries, 64 MiB offline catalog, PNG/JPEG
covers up to 2048×2048, 48 decoded covers and a 128 MiB disk artwork cache.
Use Settings to clear artwork or Triangle to refresh one game's artwork.

## Build on Linux

Install Docker and GNU Make on the host; the compiler, SDK, publishing tools, font
and image utilities are managed by the Docker build.

```sh
make image
make
make test
make validate
```

Output: `dist/PS4Game_installer-rN.pkg`, plus a JSON manifest with revision, input
fingerprint and SHA-256. The build uses LLVM 18, checksum-verified OpenOrbis 0.5.4,
pinned libjbc and a pinned LibOrbisPkg publisher built with .NET 8. Unity and Sony's
SDK are not used.

A successful build with changed inputs increments the revision. No-change builds
reuse the package. Failed builds do not consume a revision, concurrent builds are
serialized, and `make clean` retains the committed revision record. Rebuilding a
clean checkout with identical inputs retains that checkout's recorded revision.
Commit `revision.json` along with a release's source changes.

To validate your catalog from the build container without installing anything:

```sh
# Edit config.example.json with your address first.
make check CONFIG=/work/config.example.json
```

The Linux check shares parsing with the console but uses libcurl; it cannot validate
the PS4's TLS support, installed-game services or BGFT behavior.

### Deploy to your PS4

Enable the PS4's FTP server, then configure your target once:

```sh
cp .env.example .env
# Edit .env with the PS4 address, FTP credentials and destination directory.
make deploy
```

```dotenv
DEPLOY_HOST=192.168.1.50
DEPLOY_PORT=2121
DEPLOY_USER=anonymous
DEPLOY_PASS=anonymous
DEPLOY_PATH=/data/pkg
```

`make deploy` reads the repository's `.env`, builds the current revision using
Docker, verifies its SHA-256 manifest, and uploads `PS4Game_installer-rN.pkg` by
passive FTP. Create `DEPLOY_PATH` on the PS4 beforehand. The server must support
file upload and rename; uploads use a temporary name until the transfer completes.
The remote size is checked when the server supports `SIZE`. Install the uploaded
package using your PS4 package installer.

`.env` is ignored by Git and excluded from Docker build contexts. Deployment reads
it through the container's repository mount; credentials are not passed on command
lines or printed. Values may be single/double quoted, and comments are supported.
Shell commands and variable substitutions in `.env` remain literal text. Plain FTP
is intended for your trusted local network.

### Desktop preview

```sh
make preview       # Interactive X11 window when DISPLAY is available; otherwise screenshots.
make screenshots   # Always generate build/screenshots/*.png without a display.
```

Interactive preview uses the same UI and renderer with a sample library and disables
PS4 launch/install operations. It forwards your X11 socket and XAUTHORITY when set;
ensure those environment variables identify your current desktop session.
Keyboard controls: arrows = D-pad, Enter/X = Cross, Escape/O = Circle, S = Square,
T = Triangle, Tab = Options, Q/E = L1/R1. Enter accepts text; Escape cancels it.
An SDL-compatible controller can also navigate the preview.

## Data and troubleshooting

The console stores its files under `/data/PS4-Game-Installer/`:

- `config.json`: saved sources and display preferences; see config.example.json.
- `catalog.json`: last successful remote catalog snapshot.
- `tasks.json`: submitted task IDs; task cancellation remains in the PS4 system UI.
- `artwork/`: disposable cover cache.
- `app.log`: recent operation results and service errors.

On the first launch of r13 or later, this app's previous
`/data/PS4-Games-Installer/` directory is moved to `/data/PS4-Game-Installer/` when
the new directory does not exist. Saved settings, catalog, tasks and artwork stay
together. An existing new directory takes precedence.

| Problem | Check |
| --- | --- |
| Cannot open `/app0/assets/icon.png` at startup | Install r7 or later; the startup asset-loading order was corrected |
| System keyboard types with O and closes with X | Install r9 or later; package metadata now requests X confirm / O cancel for native dialogs |
| CE-34878-0 when closing the app or an error screen | Retest with r8 or later, which restores startup credentials and requests exit through the PS4 shell. The reported end-of-app crash is not yet verified fixed on hardware |
| Empty Available | Set the direct JSON URL, use Test connection, then Refresh library |
| HTTP 404 from the companion server | Use its `/games` link on the JSON library port |
| Covers or installation fail | Check the URLs inside the JSON and package/artwork server reachability |
| Installed is empty | Confirm GoldHEN is active, reconnect external storage, refresh, and inspect app.log |
| Installed titles appear without their icons | Install r10 or later, which looks up icons separately from private metadata. Use Options > Refresh library after reconnecting Extended Storage |
| Launch fails | Close another running game and check the title's storage/disc and displayed service error |
| Package already queued | Manage the existing task in PS4 Notifications > Downloads |
| Existing-title conflict | The installed title was preserved; this app does not force removal |
| Settings cannot save | Check free space and access to the application data directory |
| URL is saved but Available stays empty | Read the connection result in Settings. Use the companion server's `/games` endpoint; r12 also corrects an HTTP initialization issue seen as `Disable redirects (0x80431001)` |
| HTTPS fails | Check server certificates and firmware TLS compatibility; a private LAN HTTP server is supported |

PS5, database repair, system-menu replacement and automatic game removal are outside
this version. Runtime service behavior and frame rate still require console testing.

## Development and attribution

Application code lives in `src/`, editable assets in `assets/`, dependencies in
`third_party/`, tests in `tests/`, and build helpers in `scripts/`.
The root Makefile is the build entry point.

See [CHANGELOG.md](CHANGELOG.md), [TODO.md](TODO.md), and
[PROGRESS.md](PROGRESS.md) for changes, remaining hardware checks and validation.
Source is GPLv2; see [LICENSE](LICENSE) and [THIRD_PARTY.md](THIRD_PARTY.md).
