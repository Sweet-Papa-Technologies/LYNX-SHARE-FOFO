# Fork Architecture Map (`docs/FORK_MAP.md`)

Concise architectural map of **LYNX-SHARE-FOFO** for the areas actively modified in this fork, plus a complete inventory of where the upstream application name (`LYNX` / `LYNX SHARE`) appears so a future rename is mechanical.

---

## 1. High-Level Architecture

- **Document & DSP (`src/core/`)**: Pure C++20 with zero JUCE dependencies.
  - `lynx::Song` (`src/core/Song.h`) holds the entire project state: patterns, channels, playlist arrangements/clips, mixer inserts (`MixerTrack`) and their effect slots (`EffectSlot`), automations, and project metadata.
  - Every UI edit calls `AudioEngine::editSong(fn)` (`src/engine/AudioEngine.h`), which records an undo step in `History` (`src/core/History.h`), mutates the UI thread's `Song`, and publishes an immutable `RackState` snapshot to the real-time audio thread through `SnapshotExchange` (`src/core/SnapshotExchange.h`).
- **Persistence (`src/engine/ProjectFile.cpp`, `src/flp/`)**:
  - `.lynx` projects are serialized to/from JSON (optionally bundled as ZIP/package with samples) in `src/engine/ProjectFile.cpp`.
  - `.flp` files are parsed in `src/flp/FlpParser.h` and mapped into `lynx::Song` in `src/core/FlpImport.cpp`.
- **UI Shell (`src/ui/MainComponent.*`, `src/ui/Workspace.*`)**:
  - `MainComponent` owns the two-row FL-style top toolbar, menu bar, keyboard shortcut dispatcher (`src/core/KeyMap.h`), and a `Workspace` desktop (`src/ui/Workspace.h`).
  - `Workspace` hosts the five primary panels inside `PanelFrame` containers (or `DetachedPanelWindow` when undocked):
    1. **Playlist** (`workspace::Panel::playlist` → `PlaylistComponent` in `src/ui/PlaylistComponent.*`)
    2. **Channel Rack** (`workspace::Panel::channelRack` → `ChannelRackComponent` in `src/ui/ChannelRackComponent.*`)
    3. **Piano Roll** (`workspace::Panel::pianoRoll` → `PianoRollComponent` in `src/ui/PianoRollComponent.*`)
    4. **Mixer** (`workspace::Panel::mixer` → `MixerComponent` in `src/ui/MixerComponent.*`)
    5. **Browser** (`workspace::Panel::browser` → `BrowserComponent` in `src/ui/BrowserComponent.*`)

---

## 2. UI Theme & LookAndFeel

| File | Responsibility |
|---|---|
| `src/core/ThemePalette.h` / `src/core/ThemePalette.cpp` | Pure C++20 color math (WCAG 2 relative luminance, contrast ratio, `readableTextOn`, `mix`, hex parsing) and `themes::Palette` / `themes::Spec`. `derivePalette(accent, background)` computes surface elevations (`bg0`..`bg6`), borders, text colors, accent states, step sequencer colors, and piano roll colors. `builtInThemes()` defines the built-in preset list. |
| `src/ui/Theme.h` / `src/ui/Theme.cpp` | Global live `juce::Colour` design tokens (`lynx::theme::colour::*`), typography (`theme::type::*`), spacing/radius constants, custom knob/digit-display painters, and `lynx::theme::LookAndFeel` (subclass of `juce::LookAndFeel_V4`) which styles all stock JUCE widgets, menus, buttons, sliders, and scrollbars. `theme::setPalette()` updates the live tokens. |
| `src/ui/ThemeSettings.h` / `src/ui/ThemeSettings.cpp` | Loads/saves `<settings>/theme.lynxtheme` and user presets in `<settings>/Themes/*.lynxtheme`, implements `themeui::apply()` (which updates `theme::setPalette()`, calls `LookAndFeel::refreshColours()`, and remaps component colors across all open windows), paints the workspace backdrop (`paintWorkspaceBackdrop`), and provides the *Options › Theme* settings page (`makeThemePage()`). |
| `src/ui/Workspace.cpp` (`PanelFrame`) | Draws each main panel's title bar (`Workspace::titleHeight`), focus indicator bar along the top edge (`paintOverChildren`), title text (`TitleLabel`), and panel frame border. |
| `src/ui/PluginSkin.h` / `src/ui/PluginSkin.cpp` | Per-plugin/editor visual skinning tokens used by native instrument and effect editors. |

---

## 3. Recording & Audio Recorder (Edison-Style)

LYNX has two complementary audio recording paths:

### A. Playlist / Disk Take Recording
- **`src/core/Recorder.h` & `src/core/RecordLoop.h`**: Note/automation recording state machine, loop-pass overdub/blend logic, count-in catch (`CountInCatch`), and punch-in/out ranges (`PunchRange`).
- **`src/core/TakeRecording.h` / `src/core/TakeRecording.cpp`**: Lock-free SPSC disk writer (`takes::DiskRecorder`), crash-recovery journals (`takes::Journal`), round-trip latency ping (`takes::LatencyPing`), and WAV header finalization.
- **`src/engine/AudioEngine.cpp`**: Orchestrates armed mixer inserts (`armedInserts`), streams input/insert audio to `DiskRecorder`, and places completed WAV clips onto the Playlist when recording stops (`--rectest` in `src/Main.cpp` exercises this headlessly).

### B. Edison-Style Audio Editor & Recorder (`AudioEditor`)
- **`src/core/AudioCapture.h`**: Lock-free SPSC ring buffer (`AudioCapture`, owned by `MeterBank::capture` in `AudioEngine`). The audio thread pushes stereo frames from the selected tap source (`AudioCapture::input = -2`, `0 = Master`, `1..N = Insert 1..N`); the UI thread pulls frames on a timer.
- **`src/core/AudioEdit.h` / `src/core/AudioEdit.cpp`**: Pure C++20 `lynx::audioedit::Document` (stereo sample buffers, `sampleRate`, `Marker` list with `{position, name, length, endInclusive}`) and sample-accurate editing operations (trim, delete, fade, normalize, reverse, silence, declick, loop/region selection, marker slicing, undo history).
- **`src/ui/AudioEditor.h` / `src/ui/AudioEditor.cpp`**: The Edison-style waveform editor and recorder UI (`lynx::AudioEditor`):
  - Supports 4 recording trigger modes (`RecordMode::now`, `onInput`, `onPlay`, `inputOnly`), `Append` vs replace, input gain, noise threshold, and source selector (Audio Input, Master, or any Mixer Insert).
  - Pulls captured audio from `engine.getMeters().capture` in `timerCallback()`, displays live waveform + ruler + markers, and supports dragging regions/samples directly onto the Playlist or Channel Rack, exporting WAV, or sending to the arrangement.
  - Entry points: `openAudioEditor(engine, file)`, `openAudioEditorRecording(engine)`, `openAudioEditorForInsert(engine, insertIndex)`, and the record modal ("What would you like to record?" → *Audio, into the audio editor (Edison)*).

---

## 4. Plugin Catalog, Pickers & Mixer Effect Menus

| File | Responsibility |
|---|---|
| `src/core/Song.h` (`InstrumentKind`, `EffectKind`) | Enumerates every built-in generator (`InstrumentKind`) and mixer effect (`EffectKind`), plus `EffectSlot` state (`kind`, `enabled`, `mix`, `params`, `plugin`, etc.). |
| `src/core/Effects.h` / `src/core/Effects.cpp` | `addableEffects()` returns the ordered list of `EffectKind`s shown in mixer effect menus; `effectInfo(EffectKind)` returns `{name, shortName, category, parameterSpecs}` for each effect; `makeEffect(EffectKind)` instantiates the real-time processor. |
| `src/core/PluginCatalog.h` / `src/core/PluginCatalog.cpp` | Builds the unified catalog of built-in generators, built-in effects (via `addableEffects()`), and scanned VST/VST3/CLAP/AU/LV2 plugins (`buildCatalog()`), including FL Studio category folders (`flGeneratorFolders()`, `flEffectFolders()`), search filtering (`selectEntries()`), and favorites. |
| `src/ui/QuickPick.h` / `src/ui/QuickPick.cpp` | Builds categorized popup menu entries for adding channels (`instrumentEntries()`) and mixer effects (`effectEntries()`), grouping `addableEffects()` by `EffectInfo::category`. |
| `src/ui/MixerComponent.cpp` | Calls `QuickPick::effectEntries()` to populate the mixer insert effect-slot menu (`showSlotMenu`), and calls `NativeEffectWindows::open(engine, track, slot)` when an effect slot is clicked. |
| `src/ui/PluginEditors.cpp` (`NativeEffectWindows`) | Dispatches `NativeEffectWindows::open(engine, track, slot)` to custom editor windows (`PatcherWindows`, `VisualWindows`, `FlExtraWindows`, `NewtoneWindows`, `FacetEqWindows`, `ProEditorWindows`, or generic `NativeEffectEditor`). |
| `src/ui/PluginPickerView.cpp` | Full-screen visual plugin picker (`F8` / toolbar button) with thumbnail cards (`src/ui/PluginThumbnails.cpp`), category tabs, search, and drag-and-drop onto the Channel Rack or Mixer. Uses `catalog::buildCatalog()`. |
| `src/ui/PluginPicker.cpp` | Modal plugin picker dialog (`openPluginPickerForChannel`, `openPluginPickerForSlot`) backed by `catalog::buildCatalog()`. |
| `src/ui/QuickAddPalette.cpp` | Command/plugin quick-add palette (`Ctrl+P` / right-click on picker toolbar button) backed by `catalog::buildCatalog()` plus app actions. |

---

## 5. Inventory of App Name Occurrences (`LYNX` / `LYNX SHARE`)

Per Phase 1.5, the app is **not** renamed yet. Below is every location where the product name, binary name, file extension, registry key, or bundle identifier appears so a future rename is mechanical:

### Build System & Binary Targets
- `CMakeLists.txt`:
  - `project(LYNX VERSION 0.1.24 LANGUAGES C CXX)` (line 20)
  - `juce_add_gui_app(LYNX PRODUCT_NAME "LYNX" ...)` (lines 360–369): sets `PRODUCT_NAME`, `COMPANY_NAME`, `BUNDLE_ID "com.lynx.daw"`, icon files.
  - Optional plugin targets (`LYNX_BUILD_PLUGINS`): `LYNX_Instrument`, `LYNX_Effect` (`BUNDLE_ID "com.lynx.plugins"`).
  - Windows ASIO & 32-bit bridge targets: `lynx-asio`, `lynx-asio32`, `LYNX-bridge32`.
- `scripts/build-local.sh`, `scripts/build-windows-zig.sh`, `scripts/package-macos.sh`, `scripts/package-linux.sh`, `scripts/make-installer.sh`, `scripts/make-msix-layout.py`: output binary names (`LYNX`, `LYNX.exe`, `LYNX.app`), archive prefixes (`LYNX-<label>-*`), and MSIX manifest identity.
- `.github/workflows/tests.yml`, `.github/workflows/release.yml`: artifact names (`LYNX-*`), binary paths, and `VersionInfo.ProductName` checks (`'LYNX SHARE'`).

### Application Identity, Paths & File Associations
- `src/Main.cpp`:
  - `getApplicationName()` returns `"LYNX SHARE"` (line 2140).
  - Main window title `"LYNX SHARE"` (`MainWindow` constructor and `MainComponent::updateTitle()`).
  - CLI help/version strings and `--showcase` demo metadata.
- `src/platform/Paths.cpp`:
  - `settingsFolder()`: `%APPDATA%\LYNX` (Windows), `~/Library/Application Support/LYNX` (macOS), `~/.config/LYNX` (Linux), or `$LYNX_PROFILE_DIR/LYNX`.
  - `cacheFolder()`: `~/Library/Caches/LYNX` (macOS), `~/.cache/LYNX` (Linux).
  - Environment variables: `LYNX_PROFILE_DIR`, `LYNX_TEST_MODE`, `LYNX_SHORT_PATH_RELAUNCH`.
- `src/platform/FileTypes.cpp`:
  - Windows registry ProgIDs: `LYNX.Project` (`"LYNX SHARE Project"`), `.lynx` (`application/x-lynx-project`), `.lynxpack` (`application/x-lynx-package`).
  - Linux desktop integration: `com.lynx_share.LYNX.desktop`, `X-LYNX-AppImage=true`, `lynx-mime.xml`, `lynx.png`.
- `src/engine/ProjectFile.cpp`:
  - `.lynx` JSON header `"format": "LYNX"`, `"version": ...`.
- `src/ui/ThemeSettings.h` / `src/ui/ThemeSettings.cpp` / `src/core/ThemePalette.h`:
  - Theme file extension `.lynxtheme`, JSON `"format": "LYNX theme"`, built-in preset name `"LYNX"`.

### UI Chrome, Splash, Updater & Network User-Agents
- `src/ui/Splash.cpp`:
  - Animated splash title words `"LYNX"` and `"SHARE"` (`SplashScreen::paint`, line 242).
- `src/ui/MainComponent.cpp`:
  - `makeAboutWindow()` (About dialog title and description).
  - Window title bar formatting (`<project> - LYNX SHARE`).
- `src/ui/SetupWizard.cpp`, `src/ui/SettingsPages.cpp`:
  - Welcome text, settings descriptions, and ASIO driver descriptions mentioning `"LYNX SHARE"`.
- `src/ui/Updater.cpp`:
  - `userAgent()` (`"LYNX-SHARE/<version> (update check)"`), temp folder `"LYNX-update"`, helper `"LYNX-updater.exe"`, status bar messages.
- `src/engine/LiveSessions.cpp`:
  - `userAgent()` (`"LYNX-SHARE/<version>"`), media token prefix `"lynx-media:"`, invite code prefix `"LYNX-"`.
- `src/plugin/Vst3Entry.cpp`, `src/plugin/ClapEntry.cpp`:
  - Hosted plugin wrapper names (`"LYNX"`, `"LYNX Effects"`, `com.lynx.*`).

### OS Resources, Installers & Translations
- `resources/LYNX.rc`, `resources/LYNX-bridge32.rc`: Windows version info (`ProductName` = `"LYNX SHARE"`, `InternalName` = `"LYNX"`, `OriginalFilename` = `"LYNX.exe"`).
- `resources/installer/LYNX.nsi`: NSIS installer (`!define APP "LYNX SHARE"`, `InstallDir "$LOCALAPPDATA\Programs\LYNX"`, Start Menu / Desktop shortcuts `"LYNX SHARE.lnk"`, registry key `Software\LYNX`).
- `resources/linux/LYNX.desktop`, `resources/linux/com.lynx_share.LYNX.appdata.xml`, `resources/linux/lynx-mime.xml`.
- `asio/Register LYNX ASIO.bat`, `asio/Unregister LYNX ASIO.bat`, `src/asio/LynxAsio.cpp` (`"LYNX ASIO"`).
- `resources/i18n/*.txt`: Localized UI strings containing `LYNX SHARE`.

---

## 5. Upstream Reference Audit (`lynx-share.com`, `Ka1-S`, `LYNX-SHARE`)

Every reference to upstream infrastructure, URLs, and identifiers audited in Phase 1:

| File & Location | Upstream Reference | Action Taken |
|---|---|---|
| `src/core/CrashReport.h` (`Consent`, `issueFormBase`), `src/core/CrashReport.cpp` (`parseConsent`) | Default `Consent::ask`; `https://github.com/Ka1-S/LYNX-SHARE/issues/new` | Changed default `Consent` to `Consent::never`; repointed `issueFormBase` to `https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/issues/new`. |
| `src/ui/CrashReporter.h`, `src/ui/CrashReporter.cpp` (`endpoint`, `post`, `send`, `retryPending`), `src/ui/SettingsPages.h` (`crashReports`) | `https://lynx-share.com/api/crash.php`, default `crashReports = 0` (`ask`) | Disabled `lynx-share.com/api/crash.php` upload (`endpoint()` returns empty unless `LYNX_CRASH_ENDPOINT` is set; `post`/`send`/`retryPending` reject when empty); set `GeneralOptions::crashReports = 2` (`never`). |
| `src/core/UpdateCheck.h` (`releasesUrl`, `releasesPage`) | `https://api.github.com/repos/Ka1-S/LYNX-SHARE/releases?per_page=10`, `https://github.com/Ka1-S/LYNX-SHARE/releases` | Repointed to `Sweet-Papa-Technologies/LYNX-SHARE-FOFO`. |
| `src/engine/LiveSessions.h`, `src/engine/LiveSessions.cpp`, `src/engine/LiveTest.cpp` | `https://relay.lynx-share.com` | Disabled default connection to upstream's relay (`options.relay` defaults to `LYNX_RELAY_URL`, empty by default). |
| `src/ui/MainComponent.cpp` (line 6474) | `https://github.com/Ka1-S/LYNX-SHARE/blob/main/docs/PLUGINS.md` | Repointed to `https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/blob/main/docs/PLUGINS.md`. |
| `src/ui/SettingsPages.cpp` (line 426) | `https://github.com/Ka1-S/LYNX-SHARE/issues/new` | Repointed to `https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/issues/new`. |
| `PRIVACY.md`, `SECURITY.md`, `CONTRIBUTING.md`, `.github/ISSUE_TEMPLATE/config.yml` | `lynx-share.com`, `relay.lynx-share.com`, `Ka1-S/LYNX-SHARE` | Updated privacy policy to reflect disabled crash/relay endpoints and repointed all repository/issue/advisory URLs to `Sweet-Papa-Technologies/LYNX-SHARE-FOFO`. |
| `.github/workflows/release.yml` (`sign`, `store` jobs) | SignPath signing (`vars.SIGNPATH_ENABLED == 'true'`) and Microsoft Store submission (`store`) | Disabled `sign` (`if: false`) and `store` (`if: false`) jobs while keeping build, self-test, package, and release jobs intact. |
| `resources/installer/LYNX.nsi`, `resources/LYNX.rc`, `resources/LYNX-bridge32.rc`, `resources/linux/com.lynx_share.LYNX.appdata.xml` | `URLInfoAbout`, `Comments`, AppStream `homepage`/`bugtracker`/`vcs-browser`/`screenshots` pointing to `Ka1-S/LYNX-SHARE` and `lynx-share.com` | Repointed to `Sweet-Papa-Technologies/LYNX-SHARE-FOFO`; preserved `Ka1-S` copyright notices in `.rc` files. |
| `scripts/package-macos.sh`, `scripts/package-linux.sh`, `scripts/appimage-update-lib.sh`, `tools/ci-round-build.sh` | `Ka1-S/LYNX-SHARE`, `zsync\|https://lynx-share.com/update/...` | Repointed repo URLs to `Sweet-Papa-Technologies/LYNX-SHARE-FOFO`; cleared `UPDATE_INFO` / `UPDATE_ADDRESS` pointing to `lynx-share.com`. |
| `CMakeLists.txt`, `LICENSE`, `THIRD_PARTY.md`, `src/plugin/Vst3Entry.cpp`, `src/plugin/ClapEntry.cpp` | `Copyright (C) 2026 Ka1-S`, author/vendor attribution | Preserved intact per AGPL-3.0 license and attribution ground rules. |
| `tests/CrashReport159Tests.inc`, `tests/Live169bTests.inc`, `tests/Update166bTests.inc` | Unit test fixtures | Updated `CrashReport159Tests.inc` for `Consent::never` default and fork GitHub issue URL; kept static parser test fixtures in `Live169bTests.inc` and `Update166bTests.inc`. |
| `website/`, `server/`, `store/`, `scripts/deploy-site.sh`, `scripts/publish-appimage-update.sh`, `docs/analise/*`, `docs/design/*` | Upstream website/VPS deployment & historical design docs | Inactive in fork builds/CI; documented here for reference. |
