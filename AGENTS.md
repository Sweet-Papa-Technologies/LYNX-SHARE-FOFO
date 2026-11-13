# AGENTS.md — LYNX-SHARE-FOFO

Contributor and agent guide for **LYNX-SHARE-FOFO** (`Sweet-Papa-Technologies/LYNX-SHARE-FOFO`), a C++20 / JUCE 8.0.4 digital audio workstation forked from `Ka1-S/LYNX-SHARE`.

## 1. Ground Rules

1. **The audio thread is sacred.** Never allocate, lock a mutex, perform file/network I/O, log, or block on the audio thread (`src/core/`, `AudioEngine::audioDeviceIOCallbackWithContext`). The UI thread edits `Song` and publishes an immutable snapshot (`RackState`) via `core/SnapshotExchange.h`.
2. **Verify with built-in headless tools** before and after every change (`build-local.sh`, `--selftest`, `--export`, `--snapshot`, `comparewav`).
3. **Compare like with like.** Never diff audio renders from a CMake per-file object build against `scripts/build-local.sh`'s single-invocation compiler build (`-ffp-contract` / inlining differences shift synth phases). Render before and after with the same binary build pipeline.
4. **One change per branch, one PR per change** targeting this fork's `main`. Never push to `upstream` (`Ka1-S/LYNX-SHARE`).
5. **License stays GNU AGPL-3.0-or-later.** Preserve all copyright notices, `LICENSE`, `THIRD_PARTY.md`, and FL Studio / Image-Line trademark disclaimers.
6. **Out of scope unless explicitly requested:** audio drivers, device handling, and latency (`src/asio/`, `src/engine/WasapiDevice.*`, `src/engine/PipeWireDevice.*`).

## 2. Building

### JUCE-Free Core & Unit Tests (macOS / Linux / Git Bash / WSL)
Compiles `src/core/`, `src/flp/`, `tools/flpdump`, `tools/renderwav`, `tools/comparewav`, and the 1.95M+ check test suite without JUCE:
```bash
TMPDIR=/tmp PATH=/usr/bin:$PATH ./scripts/build-local.sh
# Run a single test by substring:
TMPDIR=/tmp LYNX_TEST=crashreport ./build-local/lynx_tests
# Optional: pass a folder of .flp files to run import/render checks against them
TMPDIR=/tmp PATH=/usr/bin:$PATH ./scripts/build-local.sh "/path/to/flp-folder"
```
Outputs go to `build-local/` (`lynx_tests`, `renderwav`, `comparewav`, `flpdump`).

### Full JUCE App — macOS / Linux (CMake + Ninja)
```bash
# If JUCE 8.0.4 is already unpacked locally (e.g. /tmp/juce-8.0.4/juce-src):
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/tmp/juce-8.0.4/juce-src
cmake --build build --target LYNX
```
- **macOS binary:** `build/LYNX_artefacts/Release/LYNX.app/Contents/MacOS/LYNX`
- **Linux binary:** `build/LYNX_artefacts/Release/LYNX`

### Full JUCE App — Windows (MSVC or Zig Cross-Compile)
```powershell
# Native MSVC 2022 (Developer PowerShell):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target LYNX
# Binary: build\LYNX_artefacts\Release\LYNX.exe
```
Or cross-compile from Linux/macOS with Zig 0.14.1: `./scripts/build-windows-zig.sh`.
*(In the FoFo home lab, Windows testing can also run on `infinity` (`192.168.1.99`) or `warlock` (`warlock.local`) via SSH; see `~/.claude/skills/fofo-home-lab/SKILL.md`.)*

## 3. Headless Verification & Regression Checks

Run all headless UI/engine checks with `LYNX_TEST_MODE=1` and `LYNX_PROFILE_DIR=/tmp/lynx-profile` (suppresses network calls, modal OS dialogs, and file-type registration, and isolates settings/cache to `/tmp`):

```bash
BIN=build/LYNX_artefacts/Release/LYNX.app/Contents/MacOS/LYNX   # or build\LYNX_artefacts\Release\LYNX.exe

# 1. Full self-test (offline render, .flp round-trip, undo/redo, mixer routing, Demo.lynx generation)
LYNX_PROFILE_DIR=/tmp/lynx-profile LYNX_TEST_MODE=1 "$BIN" --selftest selftest.txt Demo.lynx

# 2. Recorder & take-recording self-test
LYNX_PROFILE_DIR=/tmp/lynx-profile LYNX_TEST_MODE=1 "$BIN" --rectest rectest.txt

# 3. Render a .lynx or .flp project to WAV (32-bit float) and compare renders
LYNX_PROFILE_DIR=/tmp/lynx-profile LYNX_TEST_MODE=1 "$BIN" --export Demo.lynx out.wav
./build-local/comparewav baseline.wav out.wav

# 4. Headless UI screenshots (Playlist, Channel Rack, Piano Roll, Mixer, Browser, plugins)
LYNX_PROFILE_DIR=/tmp/lynx-profile LYNX_TEST_MODE=1 "$BIN" --snapshot /tmp/snap_prefix Demo.lynx

# 5. File › Save (written on its own thread while the window keeps answering): timings, the "Saving..." toolbar
#    picture (savetest-saving.png), edits and second saves made meanwhile, byte-identical project file.
#    Optional last argument: seconds of Edison recording in the test project (default 60 = a 31 MB project file).
LYNX_PROFILE_DIR=/tmp/lynx-profile LYNX_TEST_MODE=1 "$BIN" --savetest /tmp/savetest/savetest.txt
```

## 4. Source Layout

| Directory | Contents |
|---|---|
| `src/core/` | **Pure C++20 (no JUCE).** `Song` document model (`Song.h`), lock-free `SnapshotExchange.h`, DSP & synthesis (`RackRenderer.*`, `Effects.*`, `FlSynths.*`), `Recorder.h`, `TakeRecording.h`, `AudioEdit.h` (Edison-style document/ops), `AudioCapture.h`, `PluginCatalog.*`, `ThemePalette.*`, `CrashReport.*`, `UpdateCheck.*`. |
| `src/flp/` | **Pure C++20 (no JUCE).** FL Studio `.flp` binary parser and writer (`FlpParser.h`, `FlpWriter.h`). |
| `src/engine/` | **JUCE audio & I/O layer.** `AudioEngine.*` (device callback, transport, offline render, recording orchestration), `ProjectFile.*` (`.lynx` JSON serialization), `PluginHost.*` & `PluginSandbox.*` (VST3/CLAP/AU/VST2 hosting), `SampleLibrary.*`, `LiveSessions.*`. |
| `src/ui/` | **JUCE GUI layer.** `MainComponent.*` (toolbar, menus, shortcuts, panel orchestration), `Workspace.*` (floating/docked `PanelFrame`s), `PlaylistComponent.*`, `ChannelRackComponent.*`, `PianoRollComponent.*`, `MixerComponent.*`, `BrowserComponent.*`, `AudioEditor.*` (Edison-style recorder/editor), `PluginPickerView.*`, `PluginPicker.*`, `QuickAddPalette.*`, `QuickPick.*`, `Theme.*`, `ThemeSettings.*`, `CrashReporter.*`, `Updater.*`. |
| `src/platform/` | OS integration: `Paths.*` (settings/cache directories), `FileTypes.*`, `CrashGuard.*`, `Package.*`. |
| `tests/` | `tests/tests.cpp` and `tests/*Tests.inc` — JUCE-free unit & DSP tests built by `scripts/build-local.sh`. |
| `tools/` | Standalone CLI utilities: `renderwav.cpp`, `comparewav.cpp`, `flpdump.cpp`, `testplugin/`. |
| `docs/` | Architecture (`ARCHITECTURE.md`), fork map (`FORK_MAP.md`), status (`STATUS.md`), and historical analysis notes (`docs/analise/`, partly in Portuguese). |
