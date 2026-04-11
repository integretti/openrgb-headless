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

# 4. Push and let CI verify Windows + Linux
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

## Verifying after a merge

The CI workflow at `.github/workflows/headless.yml` builds Windows + Linux on
every push. Wait for both green before declaring the merge done.

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
