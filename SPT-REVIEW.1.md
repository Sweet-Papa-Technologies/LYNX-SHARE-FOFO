# LYNX-SHARE-FOFO — Agent Brief

You are working on **LYNX-SHARE-FOFO**, FoFo's personal fork of LYNX SHARE (upstream: `github.com/Ka1-S/LYNX-SHARE`), a C++20 / JUCE 8 DAW that copies FL Studio's workflow and imports `.flp` projects.

This fork is a permanent, independent line. We do **not** plan to merge back upstream, and we won't routinely pull upstream commits. Upstream fixes may be cherry-picked one at a time when FoFo asks.

Work through the phases in order. Don't start a phase until the previous one passes its checks.

---

## Ground rules (read before touching code)

- **The audio thread is sacred.** No allocation, locks, file I/O, logging or blocking calls on the audio thread. The engine's design is lock-free: the UI edits a copy of the song and publishes an immutable snapshot to the audio thread. Keep it that way.
- **Verify with the built-in tools, not by eye.** The app runs headless; use these to check your own work:
  - `LYNX.exe --selftest report.txt [demo.lynx]` — self-test without an audio device
  - `LYNX.exe --export project.flp out.wav` / `--render` — render audio
  - `LYNX.exe --snapshot prefix project.flp` — draw the UI and its windows to PNG (use this to check UI changes)
  - `tools/comparewav` — diff two renders
  - `./scripts/build-local.sh ["folder with .flp files"]` — core tests (1M+ checks, no JUCE needed)
- **Compare like with like.** The per-file object build and `build-local.sh`'s one-command build produce different (each self-consistent) bits. Never compare a render from one against a render from the other.
- **Build Windows with MSVC** (Visual Studio 2022 Build Tools + CMake), not the Zig/MinGW script. JUCE's crash guard around audio drivers (`__try`) only works under MSVC.
- **One change per branch, one PR per change.** PRs target **this fork's `main`**. GitHub defaults a fork's PRs to the upstream repo, so always check the base repository before opening a PR.
- **License stays AGPL-3.0.** Keep every existing copyright notice, `LICENSE`, `THIRD_PARTY.md` and the FL Studio / Image-Line trademark disclaimers. New files carry the same license.
- **Docs are partly in Portuguese.** Translate whatever you need as you go; don't rewrite docs wholesale.
- **Out of scope for now: audio drivers, device handling and latency.** FoFo is troubleshooting his audio setup himself. Don't change driver, device or duplex code unless he asks.

---

## Phase 0 — Orient and baseline

1. Build the app on Windows (MSVC, Release) and run `./scripts/build-local.sh`. Record the results.
2. Run `--selftest` and render 3–5 of the bundled demo projects (`Demo.lynx` plus a few `.flp` demos if FL Studio's folder is available) to WAV. Keep these as the **baseline renders** for later regression checks.
3. Write `AGENTS.md` at the repo root (keep it short, ~1 page) covering:
   - how to build (Windows MSVC, plus the core-test script)
   - how to run the tests and the headless checks above
   - the source layout: `src/core` (model, sequencer, instruments, effects, `.flp` import; plain C++), `src/flp`, `src/engine` (JUCE audio engine), `src/ui`, `tests/`, `tools/`, `docs/`
   - the ground rules above
4. Write `docs/FORK_MAP.md`: a short English map of the architecture for your own later use. Cover where the UI theme / LookAndFeel lives, where the recording and audio-recorder code lives, and where the plugin picker builds its list. Upstream's `docs/STATUS.md` is ~90 KB; summarize, don't copy.

**Done when:** the build and core tests pass, the baseline renders exist, and `AGENTS.md` plus `FORK_MAP.md` are committed.

---

## Phase 1 — Fork housekeeping

1. **Crash reports must not go to upstream's server.** Crash/freeze reports are currently POSTed to `lynx-share.com/api/crash.php`, and "Report on GitHub" pre-fills upstream's issue form. Change the default to *Never send*, remove or disable the upload, and point "Report on GitHub" at this fork's issues. Update `PRIVACY.md` to match.
2. **Find every other upstream reference.** Search the whole repo for `lynx-share.com`, `Ka1-S` and `LYNX-SHARE`, and list each hit in the PR description (website links, update checks, release workflow, code-signing config, installer metadata). Repoint or disable each one. Leave credits and attribution intact.
3. **Release workflow.** `.github/workflows/release.yml` and the SignPath code-signing setup belong to upstream. Disable the signing step for now (FoFo will decide on signing later) and make sure CI still builds and self-tests.
4. **Git remotes.** Add upstream as a read-only `upstream` remote (`git remote add upstream https://github.com/Ka1-S/LYNX-SHARE.git`) so single fixes can be cherry-picked later. Never push to it.
5. **Do NOT rename the app yet.** FoFo hasn't picked a name. When he does, the rename will cover the window title, executable, installer, settings folder (`%APPDATA%\LYNX`), project file type and the "LYNX ASIO" driver registration, so the fork never collides with an upstream install. For now, just list everywhere the name appears in `docs/FORK_MAP.md`.

**Done when:** no network call goes to upstream infrastructure, CI is green, and the baseline renders still match (`comparewav`).

---

## Phase 2 — Features

### 2.1 Audio recorder easy to find (Edison-style)

**Problem:** The audio recorder/editor (the Edison equivalent) doesn't appear in the plugin list at all. The only way to reach it is to hit Record and pick it from the modal.

**Want:**
- The recorder appears in the plugin picker / quick-add palette (F8) and in the mixer effect-slot menu, like any other effect, so it can be loaded onto any insert.
- Opening it from the record modal still works.

### 2.2 Loop-take markers in the recorder

**Problem:** When loop recording, no markers are added for each pass of the loop. You can't tell which part of the waveform belongs to which take.

**Want (match FL Studio's Edison behavior):**
- Each time the song/pattern loop wraps during recording, add a **vertical marker** at that exact sample position.
- Markers are labeled (Take 1, Take 2, …).
- **Clicking a take** (marker or the region between two markers) selects exactly that loop's region, sample-accurate, ready to drag to the Playlist, export or delete.
- Markers are saved with the project and survive undo/redo.

**Verify:** a headless test that loop-records a known signal for N passes and asserts N−1 (or N) markers at the expected sample positions, plus a `--snapshot` PNG showing the markers.

### 2.3 Colorful, readable theme

**Problem:** The current UI colors feel flat. FoFo wants something **more colorful while still easy to read and navigate**.

**Want:**
- A new default theme: vivid accent colors per window type (Playlist, Channel Rack, Piano Roll, Mixer, Browser) with strong text contrast (aim for WCAG AA on text) and clear focus/selection states.
- Build it on the existing theming system (accent color, background, workspace image), not a rewrite.
- Keep the current theme available as an option.

**Verify:** `--snapshot` PNGs of every main window, before and after, attached to the PR for FoFo to review. Don't merge a theme change without his sign-off on the screenshots.

---

## Reporting

At the end of each phase, write a short summary in the PR: what changed, how it was verified (commands and results), and anything you were unsure about. If something in this brief turns out to be wrong about the codebase, say so instead of working around it.
