# Maintaining the openrgb-headless fork

This fork tracks `gitlab.com/CalcProgrammer1/OpenRGB` upstream and applies a small
set of deletions / edits to strip Qt and the GUI. The goal is to make upstream
syncs cheap so we keep getting new device controllers, hardware detector
improvements, and SDK protocol fixes for free.

## Branch layout

- `main` — mirrors `upstream/master`. Should never have headless-specific
  changes. Merging upstream goes into this branch first.
- `headless` — the headless build. Branches off `main` and contains all the
  deletions, the rewritten `OpenRGB.pro`, the rewritten `startup/startup.cpp`,
  and the headless `README.md` / `MAINTAINING.md`. CI publishes the binary from
  this branch.

## Initial fork setup (already done)

```bash
git remote add upstream https://gitlab.com/CalcProgrammer1/OpenRGB.git
git fetch upstream
git checkout -b main upstream/master
git push -u origin main
git checkout -b headless main
# ... apply patches, delete GUI files ...
git push -u origin headless
```

## Routine upstream sync

Run this every few weeks (or whenever upstream tags a release).

```bash
cd /path/to/openrgb-headless
git fetch upstream

# 1. Update main to upstream's master
git checkout main
git merge upstream/master --ff-only         # should always fast-forward
git push origin main

# 2. Merge main into headless and resolve
git checkout headless
git merge main
# Resolve conflicts:
#  - OpenRGB.pro almost always conflicts (we restructured it).
#    Take both sides intelligently: keep our headless edits, integrate any
#    new SOURCES/HEADERS lines for new controllers from upstream.
#  - startup/startup.cpp may conflict if upstream changed it.
#    Take our version unless upstream changed something fundamental.
#  - README.md / MAINTAINING.md — always take ours.
#  - Any new files under qt/ that upstream added — `git rm` them and re-commit.

# 3. Build and verify locally before pushing
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc)
./openrgb --server --server-port 6742 &
nc -z 127.0.0.1 6742 && echo "server ok"
kill %1

# 4. Push and let CI verify Windows + Linux + macOS
git push origin headless
```

## Files we own (always keep ours on conflict)

- `README.md`
- `MAINTAINING.md`
- `.github/workflows/headless.yml`
- `OpenRGB.pro` (merge intelligently — see below)
- `startup/startup.cpp` (we deleted the GUI branches)
- `startup/main_Windows.cpp` (we removed the QApplication include)
- `startup/main_FreeBSD_Linux_MacOS.cpp` (we removed macutils.h include)

## Files we deleted (re-delete on conflict)

If upstream re-adds any of these in a merge, `git rm` them again:

- `qt/` — entire directory
- `dependencies/ColorWheel/` — entire directory
- `SuspendResume/` — entire directory
- `PluginManager.cpp`, `PluginManager.h`
- `OpenRGBPluginInterface.h`
- `Documentation/Images/` — GUI screenshots
- `README-HEADLESS.md` — superseded by `README.md`

## Files we relocated

- `qt/hsv.{cpp,h}` → `hsv.{cpp,h}` (top level). Several controllers
  `#include "hsv.h"`. Upstream merges that touch `qt/hsv.cpp` will conflict —
  apply the change to the top-level `hsv.cpp` instead.

## OpenRGB.pro conflict-resolution playbook

When upstream merges touch `OpenRGB.pro`:

1. **Upstream added a controller**: take their new `Controllers/...cpp/.h`
   lines into our SOURCES/HEADERS. Our deletions don't affect controller files.
2. **Upstream added a new GUI file**: drop their addition. Our `OpenRGB.pro`
   doesn't reference `qt/`, FORMS, RESOURCES, TRANSLATIONS, ColorWheel, or
   PluginManager — keep it that way.
3. **Upstream changed Qt version requirements** (`QT += widgets-foo`): drop
   their addition. Our `QT =` line clears all Qt modules.
4. **Upstream changed `lrelease` / `embed_translations` / icon / desktop
   install rules**: drop their addition. We don't ship a desktop entry.
5. **Upstream changed dependencies** (`hidapi`, `libusb`, `mbedtls`): take
   their changes. We use the same hardware backends.
6. **Upstream changed CLI flags** in `cli.cpp`: take their changes. We support
   the same CLI surface; our `startup/startup.cpp` ignores GUI-only flags.

## Fork-specific patches (not upstream-identical)

### ResourceManager.cpp - detector exception safety + per-detector timeouts

**What we changed:**

1. Added `#include <future>` to the includes.
2. Added a static helper `RunDetectorWithTimeout(fn, name, timeout_ms)` and
   constant `DETECTOR_TIMEOUT_MS = 5000` defined just above
   `DetectDevicesCoroutine()`. The helper runs the detector callback on a
   worker thread; if it doesn't return within `timeout_ms`, the helper logs
   the timeout, detaches the thread, and returns. Exceptions are also caught
   and logged.
3. Replaced all 8 raw detector-invocation sites in `DetectDevicesCoroutine()`
   with calls to `RunDetectorWithTimeout([&]() { <original call>; }, ...)`.
   Sites: I2C device, I2C DIMM, I2C PCI, HID safe-mode, HID normal, HID
   wrapped (normal), HID wrapped (libusb/Linux), and miscellaneous device.

**Why:** Upstream's detection loop runs detectors sequentially with no fault
isolation. Two failure modes break the entire detection pass:

- **Exceptions:** `std::bad_alloc` from `new`, `std::runtime_error` from DMI
  reads, etc. - the coroutine unwinds, `DetectDeviceMutex` is leaked, and all
  later detectors never run.
- **Hangs:** A detector matches a USB device by VID/PID but the device
  doesn't speak the expected protocol. The detector's `hid_get_feature_report`
  blocks forever and detection stops mid-pass. Observed in practice with the
  Corsair M65 PRO detector matching a non-Corsair device on VID `1B1C` PID
  `1B2E`.

The timeout/exception helper isolates each detector. Failures log a single
`LOG_ERROR` and the next detector still runs.

**Detached-thread caveat:** A timed-out worker thread is detached, not killed
(C++ has no portable thread-cancel). It continues holding its HID handle
until the underlying syscall returns. This is acceptable: the alternative is
the entire detection pass blocking forever.

**Conflict resolution:** If upstream touches one of the detector-invocation
lines, merge their change into the body of the lambda. The helper signature
stays the same. Pattern:

```cpp
RunDetectorWithTimeout(
    [&]() { <upstream's detector call>; },
    detection_string,
    DETECTOR_TIMEOUT_MS);
```

If upstream adds a new detector category, wrap its invocation the same way.

**Upstream PR candidate:** Yes - both the exception-handling and the timeout
mechanism are universally useful. SignalRGB famously isolates detectors;
upstream OpenRGB would benefit from the same. If accepted, we drop this patch
on the next sync.

### ResourceManager.cpp - detection-failure placeholder controllers

**What we changed:**

1. Added `#include "RGBController_Dummy.h"` near the other resource-manager
   includes.
2. Added a static helper `RegisterDetectionFailurePlaceholder(const char*)`
   just above `RunDetectorWithTimeout`. It `new`s an empty
   `RGBController_Dummy`, sets `name = detector_name` and `type =
   DEVICE_TYPE_UNKNOWN`, leaves zones/modes/leds empty, and registers it via
   `ResourceManager::get()->RegisterRGBController`.
3. `RunDetectorWithTimeout` now calls the placeholder helper on both failure
   paths: after `future.get()` returns false, and after the timeout `detach`.

**Why:** Without this, a detector that times out or throws leaves no trace
in the SDK device list - the client never learns the device was seen. The
placeholder means the name still reaches the client, and the client can use
"zero zones / zero LEDs" as an unambiguous signal that this entry is a
detection failure (informational only, not drivable). This is purely additive
to the exception/timeout patch above.

**Memory:** the placeholder lives in `rgb_controllers_hw` and is freed by the
existing `Cleanup()` / pre-detection clear path that already `delete`s every
entry between detection runs. No new lifecycle code.

**Conflict resolution:** if upstream ever adds native "detection failure"
reporting (e.g. a new SDK packet or an on-controller status flag), drop this
patch entirely and use their mechanism. Until then, keep the two call sites
inside `RunDetectorWithTimeout` in sync with any future refactor of that
helper.

**Upstream PR candidate:** Possible. If we upstream the timeout/exception
patch, the placeholder-on-failure behavior is a natural follow-on, but the
right form upstream is probably a typed diagnostic packet rather than an
empty controller entry. Not worth pushing separately.

### OpenRGB.pro - macOS headless build

**What we changed:** inside the `macx { }` block,

1. `CONFIG -= app_bundle` so the build emits a plain Mach-O console
   executable instead of a `.app` bundle.
2. Added `-framework IOKit` and `-framework CoreFoundation` to `LIBS`.

**Why:** the headless fork ships a console binary on every platform, not a
macOS `.app`. `IOKit` + `CoreFoundation` are required by
`dependencies/macUSPCIO/macUSPCIOAccess.h` (uses `IOServiceMatching`,
`kIOMasterPortDefault`, etc.); upstream used to get them transitively via
Qt's `QtGui` framework, which the headless build no longer links.

**Conflict resolution:** if upstream edits the `macx` block, keep both
changes on top. They are additive - neither line affects upstream's
macOS GUI build.

## Verifying after a merge

The CI workflow at `.github/workflows/headless.yml` builds Windows, Linux,
and macOS (arm64) on every push. Wait for all three green before declaring
the merge done.

Local smoke test:

```bash
qmake OpenRGB.pro CONFIG+=release
make -j$(nproc) 2>&1 | tail -20
./openrgb --server --noautoconnect &
sleep 2
nc -z 127.0.0.1 6742 && echo "ok" || echo "FAIL"
kill %1
```

If the local smoke test passes and CI is green, the merge is good.

## Cadence

Sync upstream every **2-3 months** (matches upstream's release cadence). More
frequent syncs aren't worth the conflict-resolution time. Less frequent syncs
let conflicts pile up and become harder.
