# OpenRGB Headless

This is a fork of [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) that adds a
**`headless` build configuration** which strips the entire Qt dependency (Qt5Widgets,
Qt5Gui, Qt5Core, Qt5DBus). The result is a small Qt-less binary that exposes the
OpenRGB SDK over its standard TCP port (default `6742`) and nothing else: no GUI,
no plugin loader, no system tray.

Upstream OpenRGB has [an open feature request for a headless mode](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/2012)
that has been pending for years. This fork implements it as a minimal patch on top
of upstream so it can be re-applied easily on every release.

## Why

The official Windows portable build is ~13 MiB compressed and ~25 MiB extracted —
most of which is Qt5 runtime DLLs the SDK server doesn't actually use at runtime.
A `--server`-only build with Qt removed is **~3 MiB total**.

## License

OpenRGB is licensed under the **GNU General Public License, version 2 or later**
(GPL-2.0-or-later). This fork inherits the same license. The patches are
[viewable in the diff](https://github.com/integretti/openrgb-headless/compare/main...headless)
and you can rebuild the binary yourself from the instructions below.

The original `LICENSE` file in the repo root applies to the entire codebase. The
upstream project copyright belongs to Adam Honse (CalcProgrammer1) and contributors.

## What was changed vs. upstream

The patch surface is intentionally tiny — just enough to make the codebase build
without Qt when `CONFIG+=headless` is passed to qmake.

| File | Change |
|---|---|
| `OpenRGB.pro` | Adds a `CONFIG(headless)` branch that does `QT -= core gui widgets`, drops the entire `qt/*` GUI source set, the plugin loader (`PluginManager.{cpp,h}`), the colour-wheel widget (`dependencies/ColorWheel`), and the per-platform suspend/resume listeners. |
| `startup/startup.cpp` | Wraps `#include <QApplication>`, `#include "OpenRGBDialog.h"`, the Linux `sigHandler` (which calls `qApp->quit()`), and the entire GUI block of `startup()` in `#ifndef OPENRGB_HEADLESS`. Adds a Qt-free `sigHandler` that calls `std::exit(0)`. Adds a blocking server loop using `std::this_thread::sleep_for` so `main()` doesn't return immediately when no GUI event loop is present. |
| `startup/main_Windows.cpp` | Wraps the unconditional `#include <QApplication>` in `#ifndef OPENRGB_HEADLESS`. The rest of the Windows entry point is already pure Win32 + std::thread. |

That's it. No controller files, no NetworkServer, no ResourceManager, no
SettingsManager touched. Upstream's core library is already 100% Qt-free, so
nothing else needs patching.

## What is removed in headless builds

- **GUI** — `qt/*` (OpenRGBDialog, OpenRGBDevicePage, all forms, all themes)
- **Plugin loader** — `PluginManager` (uses `QPluginLoader` and the QWidget plugin ABI)
- **ColorWheel widget** — only used by the GUI
- **Suspend/resume listeners** — `SuspendResume_Windows.cpp` (uses `QAbstractNativeEventFilter` from Qt5Core), `SuspendResume_Linux_FreeBSD.cpp` (uses `QDBusConnection` from Qt5DBus), and the macOS one for symmetry. The host that embeds the headless server is expected to detect OS power events itself and bounce the subprocess on resume.
- **Translations** — `lrelease`/`embed_translations` (Qt-only)

## What still works

- Every device controller (~183 of them) — they're all Qt-free in upstream
- The full OpenRGB SDK TCP protocol on port 6742
- All CLI flags that don't require a GUI (`--server`, `--server-port`,
  `--noautoconnect`, `--nodetect`, `--config`, `--profile`, `--list-devices`, etc.)
- All hardware detectors (HID, I2C, SMBus, serial)
- Cross-platform: Windows, Linux

## Building

### Windows (MSVC + qmake)

```cmd
qmake OpenRGB.pro CONFIG+=release CONFIG+=headless
nmake
```

You still need qmake itself (Qt5 install) to drive the build, but the resulting
binary links zero Qt libraries.

### Linux

```bash
sudo apt install qtbase5-dev libusb-1.0-0-dev libhidapi-dev libmbedtls-dev
qmake OpenRGB.pro CONFIG+=release CONFIG+=headless
make -j$(nproc)
```

Verify the binary is Qt-free:

```bash
ldd ./openrgb | grep -i qt
# (should produce no output)
```

## Running

```
OpenRGB-headless --server --server-port 6742 --noautoconnect --nodetect
```

The TCP SDK server listens on the port and waits for clients. There is no GUI, no
window, no tray icon — it's a pure background process.

## Syncing with upstream

```bash
git remote add upstream https://gitlab.com/CalcProgrammer1/OpenRGB.git
git fetch upstream
git checkout main
git merge upstream/master
git checkout headless
git rebase main
# Re-apply the 3-file patch if conflicts arise (rare; the touched files change slowly)
```

## Credits

All upstream code © Adam Honse (CalcProgrammer1) and the OpenRGB contributors.
Headless build patches by [@integretti](https://github.com/integretti).
