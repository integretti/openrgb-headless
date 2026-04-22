# OpenRGB Headless

A headless fork of [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) that strips
the entire Qt dependency (Qt5Widgets, Qt5Gui, Qt5Core, Qt5DBus) and ships only the
SDK server. The result is a small, GUI-less binary that exposes the OpenRGB SDK over
its standard TCP port (default `6742`) and nothing else: no GUI, no plugin loader,
no system tray, no installer-bundled `.dll`s for desktop frameworks.

The 183 device controllers and the SDK protocol are unchanged from upstream.

| Build | Size |
|---|---|
| Upstream OpenRGB Windows portable | ~13 MiB download / ~25 MiB extracted |
| This fork (Windows x64, with hidapi/libusb/PawnIO DLLs) | **~7.4 MiB** |
| This fork (Linux x64, dynamic) | **~11 MiB** |
| This fork (macOS arm64, dynamic, Homebrew dylibs) | **~8.9 MiB** |

## Why

Upstream OpenRGB has [an open feature request for a headless mode](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/2012)
that has been pending for years. This fork implements it as the smallest possible
patch on top of upstream. It exists for embedding OpenRGB inside other services as
a child process — the host speaks the SDK over loopback TCP and gets full
device-control access without bundling 12+ MiB of Qt runtime DLLs the SDK server
never actually uses.

The fork stays close to upstream so it can keep merging upstream device-controller
work, hardware detector improvements, and SDK protocol fixes. See
[`MAINTAINING.md`](MAINTAINING.md) for the upstream-sync workflow.

## What was removed vs. upstream

Everything that exists only to drive a graphical interface:

- `qt/` — every dialog, page, model, dialog .ui form, theme, font, icon, translation
- `dependencies/ColorWheel/` — pure `QWidget` colour-picker, only used by the GUI
- `PluginManager.cpp/h` and `OpenRGBPluginInterface.h` — the GUI plugin loader (uses
  `QPluginLoader` and the `QWidget` plugin ABI)
- `SuspendResume/` — every per-platform listener, all of which depended on
  `QAbstractNativeEventFilter` (Windows) or `QDBusConnection` (Linux/FreeBSD).
  The host that embeds the headless server is expected to detect OS power events
  itself and bounce the subprocess on resume.
- `Documentation/Images/` — GUI screenshots
- All Linux desktop / icon / AppStream metainfo / systemd / tmpfiles install rules
- The Windows `.exe` icon (`RC_ICONS`) — headless tools don't need one
- The macOS `.app` bundle config (Info.plist, .icns) — headless tools aren't bundles
- All translation files (`qt/i18n/*.ts`) and the `lrelease` / `embed_translations`
  qmake configs

## What stayed

Everything that's not GUI-bound:

- All ~183 device controllers under `Controllers/`
- The full OpenRGB SDK TCP protocol (`NetworkServer.cpp`, `NetworkProtocol.cpp`)
- All hardware detectors (`i2c_smbus/`, `hidapi_wrapper/`, `serial_port/`,
  `interop/`, `SPDAccessor/`)
- `ResourceManager`, `SettingsManager`, `ProfileManager`, `LogManager` — all
  Qt-free in upstream
- The `cli.cpp` CLI parser — flags that don't make sense headless
  (`--gui`, `--start-minimized`, `--client`) are accepted but ignored
- Cross-platform: Windows, Linux, and macOS (arm64) all build and ship binaries from CI
- The qmake build system — kept as-is so upstream merges remain straightforward

## License

OpenRGB is licensed under the **GNU General Public License, version 2 or later**
(GPL-2.0-or-later). This fork inherits the same license. The
[`LICENSE`](LICENSE) file in the repo root applies to the entire codebase.

The original upstream copyright belongs to Adam Honse (CalcProgrammer1) and
contributors. The headless patches are visible in [the diff against upstream
master](https://github.com/integretti/openrgb-headless/compare/main...headless).

## Building

### Windows (MSVC + qmake/jom)

```cmd
qmake OpenRGB.pro CONFIG+=release
jom -j %NUMBER_OF_PROCESSORS%
```

You still need a Qt5 install for `qmake` itself (it's the build tool), but the
resulting `OpenRGB.exe` links zero Qt libraries.

### Linux

```bash
sudo apt install qtbase5-dev libusb-1.0-0-dev libhidapi-dev libmbedtls-dev pkg-config build-essential
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc)
```

Verify the binary is Qt-free:

```bash
ldd ./openrgb | grep -i qt
# (should produce no output)
```

### macOS (Homebrew + qmake)

```bash
brew install qt@5 libusb hidapi mbedtls@3 pkg-config
export PATH="$(brew --prefix qt@5)/bin:$PATH"
qmake OpenRGB.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
```

The macOS build produces a plain Mach-O console binary (not a `.app`
bundle). Verify it links no Qt frameworks:

```bash
otool -L ./openrgb | grep -i qt
# (should produce no output)
```

CI builds for `macos-14` (Apple Silicon / arm64). x86_64 macOS is not
currently exercised by CI but the source paths are preserved.

## Running

```
OpenRGB --server --server-port 6742 --noautoconnect
```

The TCP SDK server listens on the port and accepts clients. There is no GUI, no
window, no tray icon — it is a pure background process. `--noautoconnect`
prevents auto-connecting to a remote OpenRGB instance and is the right default
for an embedded server.

CLI flags supported:

| Flag | Effect |
|---|---|
| `--server` | Run the SDK server (always implied in this fork) |
| `--server-port <n>` | Port to listen on (default 6742) |
| `--noautoconnect` | Don't auto-connect to a remote OpenRGB instance |
| `--nodetect` | Skip device detection |
| `--config <dir>` | Configuration directory (defaults to platform-specific) |
| `--profile <name>` | Load a profile at startup |
| `--list-devices` | Print detected devices and exit |
| `--loglevel <level>` | `fatal` / `error` / `message` / `verbose` / `debug` / `trace` |

GUI-only flags (`--gui`, `--start-minimized`, `--client`) are accepted but ignored.

## Maintaining

See [`MAINTAINING.md`](MAINTAINING.md) for the upstream-sync workflow. The short
version: track `upstream/master`, merge it into `main`, then merge `main` into
`headless`. Re-deletion of any GUI files upstream may have re-added is the only
recurring manual step.

## Credits

All upstream code © Adam Honse (CalcProgrammer1) and the OpenRGB contributors.
Headless build patches by [@integretti](https://github.com/integretti).
