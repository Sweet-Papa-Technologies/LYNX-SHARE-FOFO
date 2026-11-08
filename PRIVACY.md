# Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the
user or the person installing or operating it.

## What LYNX SHARE does and does not do

- **Network connections, all disclosed and under your control.** LYNX SHARE contains no telemetry, analytics,
  advertising, online account or licence check. In this fork (`Sweet-Papa-Technologies/LYNX-SHARE-FOFO`), automatic
  uploads to upstream's `lynx-share.com` servers are disabled by default:
  1. **The update check** (below): a version query to GitHub (`Sweet-Papa-Technologies/LYNX-SHARE-FOFO`), on by default, off in *Settings › General › Updates*.
  2. **Crash reports**: crash report uploads to `lynx-share.com` are disabled, and crash reporting defaults to *Never send* (local crash files only, with an option to open a prefilled GitHub issue on `Sweet-Papa-Technologies/LYNX-SHARE-FOFO`).
  3. **The stem model download**: only after you choose *Extract stems from sample* on an audio clip and agree to the
     download, once, from `https://huggingface.co` (a plain file request; Hugging Face sees your IP address). Your audio
     is never uploaded: separation runs on your computer.
  4. **Live sessions**: disabled against upstream's relay by default unless a relay URL (`LYNX_RELAY_URL`) is explicitly configured. When a relay is configured and you start (*File › Online session › Start live session...*) or
     join a session with an invite code, everything goes through that relay, **end-to-end encrypted** with a key that comes from the invite code: the relay passes the frames on
     without being able to read them (the project, your edits, your samples, your name), and it never receives the
     code's secret part. It sees what any server sees: your IP address, when you were connected and how many bytes
     went through; it keeps no log of the frames and forgets a room when the session ends. The other people in the
     session see your name, your edits, the samples your part uses and whether you are playing or recording; they
     do not see your IP address (nobody connects to your computer directly).
  The source code is public, so this can be verified: the update check in `src/ui/Updater.cpp`, the report upload
  in `src/ui/CrashReporter.cpp`, the model download in `src/ui/StemExtract.cpp` and the live session in
  `src/engine/LiveSessions.cpp` / `src/engine/LiveLink.cpp` are the only uses of a network API (JUCE's `URL` /
  `WebInputStream`, WinHTTP's WebSocket on Windows; on Linux the `curl` program and, for live sessions, the system's
  libcurl), and JUCE's web browser component and its curl support are disabled at build time
  (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`).
- **No personal data is collected.** The update check sends nothing about you or your computer; a crash report
  leaves your computer only when you choose to send it.
- **Local files only.** LYNX SHARE writes only to files you choose (projects, exports) and to its own settings folder
  `%APPDATA%\LYNX` (language, plugin list, autosaves and similar preferences). Delete that folder to remove them.
  Extracted stems go to the project's `Audio/Recorded/Stems` folder. The stem model (166 MB) is kept in
  `%LOCALAPPDATA%\LYNX\models` (macOS: `~/Library/Application Support/LYNX/models`, Linux: `~/.local/share/LYNX/models`);
  delete it to free the space, it is downloaded again (after asking) the next time you extract stems.
  It also registers the `.lynx` file type for the current user (`HKEY_CURRENT_USER\Software\Classes`) so project
  files open in LYNX SHARE.
  The Microsoft Store version keeps that folder in its own app-data folder instead
  (`%LOCALAPPDATA%\Packages\<LYNX package>\LocalCache\Roaming\LYNX`, removed when the app is uninstalled) and
  registers no file types itself (Windows does, from the package).
- **Plugin sandbox.** When a plugin is run in its own process, LYNX SHARE talks to that process through a local
  inter-process channel on the same computer; nothing is sent over a network.
- **Crash logs** are written to local files (`autosave\crash *.txt`, `hang *.txt`, `crash-history.log`).

## Update check (on by default, switchable)

A few seconds after it starts, LYNX SHARE asks GitHub's public API for the list of releases
(`https://api.github.com/repos/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/releases`) and compares the newest version number with its own. If a
newer version exists, a bar under the toolbar says so (*What's new*, *Update*, and ✕ to skip that version). The list
is kept for three hours (`update-cache.json` in the settings folder), so LYNX SHARE asks at most once every few hours;
*Help › Check for updates…* asks at any time.

- **What is sent:** an ordinary HTTPS request for that public list, with the user agent `LYNX-SHARE/<version>`. No
  identifier, no settings, no project, plugin or system information. GitHub sees your IP address, as for any visit to
  a web page; GitHub's privacy statement applies to it.
- **Turning it off:** *Settings › General › Updates › Check for updates automatically*. Off, LYNX SHARE never connects
  for updates unless you choose *Help › Check for updates…*. The choice is kept in `updates.txt` in the settings folder
  (with the version you skipped, if any).
- **Updating** happens only when you click *Update*: the new release file comes from GitHub and is checked against the
  release's `SHA256SUMS.txt` before it is used. The Windows installer version then installs it over itself (after
  asking to save your project) and starts again; the portable and macOS versions save the file to your Downloads
  folder; the Linux AppImage updates itself through AppImageUpdate when it is installed; the Linux .tar.gz opens the
  release page.
- **The Microsoft Store version never checks:** the Store updates it, so it has no update check and no *Update*
  button.

## Crash reports (local only by default)

In this fork, **Settings › General › Crash reports** defaults to *Never send*, and automatic/server uploads to upstream's `lynx-share.com` endpoint are disabled.
Crash and freeze logs are written locally (`autosave\crash *.txt`, `hang *.txt`, `crash-history.log`).
If you choose **Help › Report a problem…** or **Report on GitHub instead**, LYNX SHARE can open GitHub's issue form prefilled at
<https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/issues/new> (nothing is submitted until you submit the GitHub issue yourself in your browser).

A prefilled report / local JSON preview contains: a random report id, the LYNX SHARE version / build / commit, the operating system and its version, the
CPU, the graphics cards and their driver versions, the screen size and scaling, the audio driver / device / sample
rate / buffer size, the crash or freeze file (where in the program it stopped), the session's last 60 steps, the
names, vendors, formats and versions of the third-party plugins loaded in that session, and the "GPU rendering" and
plugin sandbox settings.

It does **not** contain: audio, projects or samples, your computer's name, your user name (every path shows `<user>`
instead), or file and folder names of projects and samples (only their extension, e.g. `<file>.flp`, unless you tick
"include file names").

## Third-party plugins

VST3 and CLAP plugins that **you** load are separate programs from other vendors. They run inside LYNX SHARE (or its
sandbox process) with your permissions and may do things LYNX SHARE does not, including network access. Their behaviour is
governed by their own privacy policies.

## Contact

Questions: open an issue at <https://github.com/Sweet-Papa-Technologies/LYNX-SHARE-FOFO/issues>.
