# Legilo - Technical Design Document

- Status: Draft 0.19
- Date: 6 November 2026
- Working title: "Legilo" is a placeholder name. Check it for clashes before publishing and replace it throughout (binary name, application ID, paths). Settle it before stage 3 (open question 1).
- Changes since the previous draft: Appendix D.

## Contents

1. Introduction
2. Goals and non-goals
3. Requirements
4. Target platforms and toolchain
5. Technology choices and dependencies
6. Architecture
7. Component design
8. Security and privacy
9. Testing strategy
10. Build, CI and packaging
11. Licensing
12. Delivery stages
13. Risks and mitigations
14. Open questions
15. Future work
- Appendix A: Settings keys
- Appendix B: Command-line reference
- Appendix C: Desktop integration files
- Appendix D: Revision history

---

## 1. Introduction

### 1.1 Purpose

Legilo is a native, offline text-to-speech (TTS) desktop application for KDE Plasma (Debian) and Windows. Its main job is to read text aloud from anywhere on the desktop with a single global shortcut: the current selection or the clipboard. It cleans the text up first, with special handling for text copied out of PDFs. A full reader window adds sentence highlighting, navigation, pasted or plain-text documents, a clean-up preview and audio export. PDF file reading and speech-to-text dictation come in later stages.

This document describes what will be built, how it is structured, the technology used, and the order in which it will be delivered.

### 1.2 Background

The design replaces an existing pair of scripts (`speak-selection`, Bash, and `speak-tts`, Python with Piper). They already prove the core idea on Plasma Wayland: reading the primary selection with `wl-paste`, cleaning PDF text, and streaming Piper audio sentence by sentence from a warm background server. Legilo keeps that behaviour, fixes the issues found in the scripts (section 7.2.6) and turns it into a packaged, cross-platform application.

### 1.3 Decisions made so far

| Topic | Decision |
|---|---|
| Platforms | KDE Plasma on Debian 13 (Wayland primary, X11 best effort) and Windows. No macOS, no GNOME for now. |
| Language and UI | C++20 with Qt 6 Widgets |
| Speech engine | sherpa-onnx (ONNX Runtime) for TTS now and speech-to-text (STT) later. No Python or PyTorch at runtime. On Linux it links Debian's ONNX Runtime (10.1). |
| Voices | Piper (VITS) and Kokoro, English only for now |
| Network | Fully offline. The only network use is model downloads the user starts. |
| GPU | Not at first |
| Linux packaging | Native `.deb` for Debian stable (13, trixie) first |
| Licence | GPL-3.0-or-later, unchanged if MuPDF is chosen at stage 10 (section 11) |
| Global shortcuts | Registered with KDE (KGlobalAccel) so they can be changed in System Settings or in the app. Win32 hotkeys on Windows. |
| First milestone | Speak selection and speak clipboard, with clean-up and PDF detection |
| Reader | Full reader window with sentence highlighting, a paste/edit box and plain-text files |
| Navigation | Start from any point, step by sentence or paragraph, resume where you stopped |
| Clean-up preview | Yes, with clean-up settings shown alongside the preview |
| PDF detection | Features and weights fitted on the regression corpus, not set by hand; the held-out set is scored once per change of features and never used to choose them (7.2.3, 9.2) |
| Audio export | Yes |
| Later | PDF files, OCR, the references-section skip, dictation, other formats, other languages |

### 1.4 Terminology

- **Selection**: the highlighted text in any application (the PRIMARY selection on X11 and Wayland; on Windows, whatever is selected in the focused control).
- **Clipboard**: the explicit copy/paste buffer.
- **Segment**: the unit of synthesis and highlighting, normally one sentence.
- **Document**: a piece of text being read, with its original text, cleaned text and segments.
- **Reflow**: rejoining lines that were broken only to fit a page width, as in text copied from PDFs.
- **Voice**: an installed TTS model plus, for multi-speaker models, a speaker ID.

---

## 2. Goals and non-goals

### 2.1 Goals

1. One keypress reads the selection or the clipboard aloud, anywhere on the desktop, with first audio within about half a second.
2. Text from PDFs is cleaned automatically and reliably, without damaging ordinary text.
3. A comfortable reader for longer text: highlighting, navigation, resume.
4. Native feel on both platforms: tray icon, system shortcuts, file manager integration.
5. Small, self-contained packages without Python, PyTorch or CUDA.
6. Fully offline and private.

### 2.2 Non-goals for this version

- macOS, GNOME, Flatpak, Snap or AppImage.
- Languages other than English.
- GPU acceleration.
- Online or cloud speech engines.
- Formats other than plain text, until the PDF stage.
- Voice cloning or training.

---

## 3. Requirements

Each requirement has an ID and the stage that delivers it (section 12).

### 3.1 Functional requirements

| ID | Requirement | Stage |
|---|---|---|
| FR-01 | Speak the current selection via a global shortcut | 3 (Linux), 5 (Windows) |
| FR-02 | Speak the clipboard via a separate global shortcut | 3, 5 |
| FR-03 | Optionally fall back from selection to clipboard when the selection is empty, with a notification saying which was used | 3 |
| FR-04 | Pressing the speak shortcut while speaking stops playback (configurable: stop, or restart with the new text) | 3 |
| FR-05 | Global shortcuts for pause/resume, stop, faster, slower, "read as PDF text" and show reader | 3 |
| FR-06 | Shortcuts editable in the app; on KDE also in System Settings, kept in sync both ways | 3 |
| FR-07 | Always-safe clean-up applied to all text (section 7.2.2) | 2 |
| FR-08 | Automatic detection of PDF-like text, with reflow applied only when detected; Auto/Always/Never override | 2 |
| FR-09 | Choice of voice and speed; speed changes take effect from the next sentence | 1, 3 |
| FR-10 | Tray icon with menu: speak selection/clipboard, pause, stop, open reader, settings, quit | 3 |
| FR-11 | Desktop notifications for state changes and errors (can be turned off) | 3 |
| FR-12 | Start automatically at login (optional) | 3 |
| FR-13 | Single instance: later launches pass their command to the running instance | 3 |
| FR-14 | Command-line interface for all actions (for scripting and fallback shortcuts) | 3 |
| FR-15 | Model manager: browse, download, verify, install, remove voices; show each voice's licence | 4 |
| FR-16 | Kokoro voices in addition to Piper | 4 |
| FR-17 | Reader window showing the text being read, with the current sentence highlighted and followed | 6 |
| FR-18 | Click to start reading from any sentence; step by sentence and paragraph | 6 |
| FR-19 | Paste or type text into the reader and read it | 6 |
| FR-20 | Open plain-text files in the reader (file dialog, drag and drop, Open With, command line) | 6 |
| FR-21 | Remember the reading position per document and offer to resume | 6 |
| FR-22 | Preview of the spoken text against the original, with differences marked | 7 |
| FR-23 | Clean-up settings shown alongside the preview, with live updates | 7 |
| FR-24 | Export a document to an audio file (WAV, FLAC, Ogg Vorbis, Opus, MP3) with progress and cancel | 8 |
| FR-25 | Optional subtitle export (SRT) alongside the audio | 8 |
| FR-26 | Read PDF files: open, drag and drop, Open With, file-manager context menu "Read aloud in Legilo" | 10 |
| FR-27 | Start a PDF from a chosen page; remove running headers and footers; handle two-column layouts | 10 |
| FR-28 | Install a voice from a downloaded archive on the command line (`--install-voice`) | 1 |
| FR-29 | System media controls: MPRIS on Linux, System Media Transport Controls on Windows (play, pause, stop, next and previous sentence) | 6 |

### 3.2 Non-functional requirements

| ID | Requirement |
|---|---|
| NFR-01 | Latency: under 500 ms from shortcut to first audio with a warm Piper "medium" voice on a typical modern 4-core CPU; under 1.5 s with Kokoro. Depends on the short first chunk (7.5); measured from stage 0 (12). The stage 0 measurements show Kokoro missing this on a 4-core 2020 laptop CPU, so the Kokoro target holds only on faster CPUs (7.3.2) |
| NFR-02 | Memory: under 300 MB resident with one Piper voice loaded; under 600 MB with Kokoro |
| NFR-03 | Tray start-up under 1 s, excluding voice loading (voices load in the background) |
| NFR-04 | No network access except user-initiated model downloads; no telemetry |
| NFR-05 | All long-running work off the GUI thread; the UI never freezes |
| NFR-06 | Robust when audio devices change: losing the output device (headphones unplugged, Bluetooth disconnected) pauses playback instead of moving it to another device, and never crashes the app or hangs playback |
| NFR-07 | The UI is fully keyboard navigable and works with the platform screen reader |
| NFR-08 | Reader handles documents of at least 5 MB of text without noticeable lag |
| NFR-09 | Text pipeline is deterministic and covered by unit tests with a regression corpus |
| NFR-10 | British English user interface strings, with translation infrastructure (Qt Linguist) ready for later |

---

## 4. Target platforms and toolchain

### 4.1 Linux

- Debian 13 (trixie), amd64. Baseline: Qt 6.8.2, KDE Frameworks 6.13, Plasma 6.3.6, GCC 14, CMake 3.31.
- Do not use any API newer than these versions. Build and test in a trixie container even when developing on trixie, so the build doesn't pick up locally installed extras.
- Plasma Wayland is the primary session. Plasma X11 should work through the same code paths (KSystemClipboard falls back to QClipboard, and KGlobalAccel works on X11) and is tested on a best-effort basis.

### 4.2 Windows

- Windows 11 x64 is the primary target. Windows 10 x64 is best effort: it reached end of support in October 2025 (extended security updates aside), but Qt 6.12 still supports it, and is the last Qt release that does.
- MSVC 2022 (v143), CMake, Ninja.
- Qt installed with the Qt online installer or `aqtinstall`. Windows bundles its own Qt, so it does not have to match Debian's 6.8.2:
  - Decided at stage 0 (open question 7): **Qt 6.12**, latest 6.12.x. Qt 6.8 open-source patch releases have ended (6.8.8 is commercial only), so the rule "newest Qt 6 release, for security fixes" applies.
  - Qt 6.12 is an LTS release, but open-source users get its patch releases only until Qt 6.13 ships (around March 2027). Then move to the newest release, which may end Windows 10 support (open question 5).
  - The Linux build enforces the 6.8 API ceiling for shared code. Windows-only code (`platform/windows`) must stay within it by review.
- Other C/C++ dependencies come from vcpkg in manifest mode.

### 4.3 Common

- C++20, CMake 3.25 minimum, CMake presets for each platform and configuration.
- `clang-format` and `clang-tidy` configuration in the repository.

---

## 5. Technology choices and dependencies

| Component | Choice | Licence | Platforms | Stage | Notes |
|---|---|---|---|---|---|
| UI framework | Qt 6, 6.8 API (Core, Gui, Widgets, Network, Concurrent, DBus on Linux, Test, LinguistTools) | LGPL-3.0 | Both | 0 | Widgets for the native look (Breeze on Plasma, Windows 11 style on Windows). DBus for MPRIS (7.7). Version per platform: 4.1, 4.2 |
| Global shortcuts (Linux) | KF6 GlobalAccel | LGPL-2.1+ | Linux | 3 | Native KDE shortcuts, shown in System Settings |
| Selection and clipboard (Linux) | KF6 GuiAddons (`KSystemClipboard`) | LGPL-2.0+ | Linux | 3 | Reads the selection without focus on Wayland via data-control |
| Speech | sherpa-onnx 2.x, pinned release | Apache-2.0 | Both | 1 | Built as a separate project from pinned archives (10.1). Since 2.0 it has no phonemiser for Piper or Kokoro; Legilo supplies phonemes (7.3.1) |
| Speech, compiled into sherpa-onnx | kaldi-native-fbank, kaldi-decoder, kaldifst, openfst, simple-sentencepiece, kissfft, eigen, utf8proc, nlohmann/json | Apache-2.0, MPL-2.0, Expat, BSD-2/3-Clause, Zlib, Unicode data | Both | 1 | Fetched by sherpa-onnx's build; pinned in `cmake/Dependencies.cmake`. Mostly for speech recognition, which sherpa-onnx cannot leave out |
| Neural network runtime | ONNX Runtime | MIT | Both | 1 | Debian's `libonnxruntime` 1.21 on Linux; a prebuilt release on Windows (stage 5) |
| Phonemisation | espeak-ng (libespeak-ng 1.52 or later) | GPL-3.0-or-later | Both | 1 | Debian's package on Linux; built from source on Windows. Used by Legilo's `Phonemiser` (7.3.1) |
| Audio output | miniaudio (single header) | Public domain / MIT-0 | Both | 1 | PulseAudio/PipeWire/ALSA on Linux, WASAPI on Windows. Debian's `libminiaudio-dev` (0.11.22) on Linux; the same release pinned in `cmake/Dependencies.cmake` on Windows (stage 5). Compiled into Legilo |
| Archive extraction | libarchive | BSD-2-Clause | Both | 1 | Model archives are `.tar.bz2`. Needed from stage 1 for `--install-voice` (FR-28) |
| Signature check | Monocypher (vendored) | BSD-2-Clause or CC0-1.0 | Both | 4 | Verifies the signed voice catalogue (7.10) |
| Text diff (preview) | diff-match-patch (C++/Qt port, vendored) | Apache-2.0 | Both | 7 | Character diff of original against cleaned text; also maps positions between texts (7.11.5, 7.12). The port predates Qt 6 and needs updating (`QRegExp`, `midRef`) |
| Audio file writing | libsndfile | LGPL-2.1+ | Both | 8 | WAV, FLAC, Ogg Vorbis, Opus, and MP3 (MP3 since libsndfile 1.1) |
| Word list | SCOWL 2020.12.07, standard size, British and American lists merged | SCOWL (permissive, notice required) | Both | 2 | Used for de-hyphenation; replaces `/usr/share/dict/words`. Compiled in as a resource from Debian's `wbritish` and `wamerican` on Linux; the same release pinned on Windows (stage 5). The larger sizes add fragments ("co", "pre") that would upset the keep-the-hyphen rule (7.2.4) |
| PDF extraction | MuPDF (preferred) or Poppler | AGPL-3.0 / GPL-2.0+ | Both | 10 | Decision point at stage 10, see section 7.15 |
| Windows selection | UI Automation (system COM API) | Platform | Windows | 5 | Fallback: simulated Ctrl+C |
| Tests | Qt Test, CTest | LGPL-3.0 | Both | 0 | |

Packages from Debian (stage 0, extended per stage):

```
build-essential cmake ninja-build pkg-config
qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
libkf6globalaccel-dev libkf6guiaddons-dev extra-cmake-modules
libarchive-dev libsndfile1-dev libespeak-ng-dev libonnxruntime-dev libminiaudio-dev
wbritish wamerican
debhelper devscripts
```

Confirm the exact names with `apt search` on trixie. Package naming for KDE Frameworks occasionally differs from the upstream framework name. `devscripts` is a developer tool and is not a build dependency of the package (10.3).

---

## 6. Architecture

### 6.1 Overview

```
+----------------------------------------------------------------------+
|                               app                                    |
|  TrayController   ReaderWindow   PreviewPane   SettingsDialog        |
|  ModelManagerDialog   ExportDialog   IpcServer   CommandLine         |
+----------------------------+-----------------------------------------+
                             | uses
+----------------------------v-----------------------------------------+
|                               core                                   |
|  text/      Normaliser, PdfDetector, Reflow, CleanupPipeline,        |
|             Segmenter, CleanupSettings                               |
|  document/  Document, Segment, PositionStore                         |
|  tts/       TtsEngine (interface), SherpaTtsEngine, Voice,           |
|             VoiceRegistry, Phonemiser                                |
|  audio/     AudioOutput (interface), MiniaudioOutput, AudioQueue     |
|  playback/  PlaybackController (state machine)                       |
|  models/    ModelCatalogue, ModelDownloader, ModelInstaller          |
|  export/    AudioExporter                                            |
+----------------------------+-----------------------------------------+
                             | interfaces implemented by
+----------------------------v-----------------------------------------+
|                             platform                                 |
|  GlobalShortcuts   TextSource   Autostart   FileIntegration          |
|  MediaControls                                                       |
|     linux/   KGlobalAccelShortcuts, KdeTextSource,                   |
|              WlPasteTextSource, XdgAutostart, MprisControls          |
|     windows/ Win32Shortcuts, UiaTextSource, RunKeyAutostart,         |
|              SmtcControls                                            |
+----------------------------------------------------------------------+
```

Rules:

- `core` depends on Qt Core, Concurrent and Network, and on the third-party libraries in section 5 (sherpa-onnx, miniaudio, libarchive, libsndfile). It uses no Widgets and calls no platform API directly.
- The speech engine and the audio device sit behind interfaces (`TtsEngine`, `AudioOutput`), so tests substitute fakes (9.1). `core` can be fully unit-tested without a display or an audio device.
- `platform` exposes abstract interfaces in `platform/include/`. CMake compiles the implementations for the target platform. Where a platform has more than one (`KdeTextSource` and the `WlPasteTextSource` contingency, 7.1.2), the choice is made at run time.
- `app` is the only layer that knows about Widgets and wires everything together.
- KDE Frameworks are linked only in the Linux build.

### 6.2 Module responsibilities

| Module | Responsibility |
|---|---|
| `core/text` | Turns raw text into cleaned text plus segments. Pure functions, no I/O. |
| `core/document` | Data model for a document being read; persistence of reading positions |
| `core/tts` | Loads voices and synthesises one segment at a time into float PCM |
| `core/audio` | Plays queued PCM, reports which segment is audible, supports pause and stop |
| `core/playback` | Orchestrates synthesis-ahead and playback; owns the state machine and commands |
| `core/models` | Voice catalogue, downloads, verification, installation, removal |
| `core/export` | Renders a document to an audio file and optional subtitles |
| `platform` | Global shortcuts, text capture, autostart, file-type registration |
| `app` | Tray, windows, dialogs, IPC, command-line parsing, settings storage |

### 6.3 Threading model

| Thread | Work | Communicates by |
|---|---|---|
| GUI (main) | Widgets, state machine, settings, IPC | Signals/slots |
| Synthesis worker (one `QThread`) | sherpa-onnx `Generate` calls, one segment at a time | Queued signals to the controller; pushes audio into `AudioQueue` |
| Audio callback (miniaudio, real time) | Copies samples to the device | Lock-free single-producer, single-consumer ring buffer; atomics for position |
| Text worker (`QtConcurrent`) | Clean-up and segmentation of large documents; diff for the preview | `QFuture` continuations |
| Network (Qt event loop) | Model downloads | `QNetworkReply` signals |
| Export worker (`QThread`) | Offline rendering to file, with its own voice instance (7.13) | Progress signals; cancellation flag |

Rules for the real-time audio callback: no allocation, no locks, no Qt calls, no logging. It reads from the ring buffer and updates `std::atomic` counters (current segment index, frames played, generation number). The GUI polls these with a 30 ms `QTimer` while playing to drive highlighting.

### 6.4 Data flow: speak selection

```
User presses shortcut
  -> GlobalShortcuts::activated("speak-selection")          [GUI thread]
  -> PlaybackController: if Starting, Playing or Paused and speakWhileSpeaking
     is stop, stop() and return (7.5)
  -> TextSource::readSelection()  -> QFuture<TextCapture>
  -> CleanupPipeline::run(capture, settings) -> Document    [text worker for large text]
  -> PlaybackController::play(document, startSegment = 0)
       -> SynthesisWorker: apply speech rules to each segment (7.2.5) and
          phonemise it (7.3.1), then
          synthesise segment 0 as a short first chunk plus the rest (7.5),
          then 1, 2 ... up to the look-ahead
       -> AudioQueue: segment PCM appended with segment index tags
       -> AudioOutput: starts the device once segment 0 is queued
  -> Highlight timer: reads the current segment and updates the reader and tray tooltip
  -> End of document: state Idle, optional notification
```

### 6.5 Repository layout

```
legilo/
  README.md                   (from stage 0)
  CHANGELOG.md                (from stage 3, the first release)
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json                  (Windows dependencies)
  cmake/                      (SherpaOnnx.cmake, Dependencies.cmake, CompilerWarnings.cmake ...)
  third_party/                (diff_match_patch, word list)
    downloads/                (pinned archives from scripts/fetch-deps; not committed)
    test-voices/              (voices for the integration tests; not committed)
  src/
    core/
      text/  document/  tts/  audio/  playback/  models/  export/
    platform/
      include/platform/       (GlobalShortcuts.h, TextSource.h, Autostart.h ...)
      linux/
      windows/
    app/
      main.cpp  TrayController  ReaderWindow  PreviewPane  SettingsDialog
      ModelManagerDialog  ExportDialog  IpcServer  CommandLine
  resources/
    catalogue/voices.json
    icons/  translations/
    linux/  (legilo.desktop, servicemenu, metainfo.xml)
    windows/ (legilo.rc, app icon)
  tests/
    unit/  integration/
    corpus/ (clean-up and detection regression samples)
  debian/                     (Debian packaging)
  packaging/container/        (trixie build image)
  packaging/windows/          (Inno Setup script)
  scripts/                    (check, build-deb, container, fetch-deps)
  docs/
```

---

## 7. Component design

### 7.1 Text acquisition

#### 7.1.1 Interface

`TextCapture` and `CaptureSource` are defined in `core` (`core/document/TextCapture.h`), not with the interface, because the clean-up pipeline takes them and `core` depends on nothing above it.

```cpp
enum class CaptureSource { Selection, Clipboard, File, Pasted };

struct TextCapture {
    QString text;
    CaptureSource source;
    QStringList mimeTypes;      // formats offered, used as a weak detection signal
    QString ownerHint;          // Windows: owning process name if known; empty on Linux
};

class TextSource : public QObject {
    Q_OBJECT
public:
    virtual QFuture<TextCapture> readSelection() = 0;   // async: Windows fallback waits
    virtual QFuture<TextCapture> readClipboard() = 0;
};
```

#### 7.1.2 Linux (KDE)

- Use `KSystemClipboard::instance()`. On Wayland it uses the data-control protocol, so it can read without window focus. On X11 it falls back to `QClipboard`.
  - Selection: `text(QClipboard::Selection)`.
  - Clipboard: `text(QClipboard::Clipboard)`.
- `mimeTypes` comes from `mimeData(mode)->formats()`.
- Known risk: trixie ships Frameworks 6.13, which uses the wlroots data-control protocol. Later versions moved to ext-data-control and fixed threading issues. Test it in the stage 0 spike (12).
  - Contingency: a `WlPasteTextSource` that runs `wl-paste --primary --no-newline` with a 1 s timeout, selected at run time if `KSystemClipboard` fails (6.1).
  - The contingency adds a dependency on `wl-clipboard` in the `.deb`.
- **Selections made from the keyboard may not reach the primary selection.** Found in stage 2: Ctrl+A in Firefox's PDF viewer and in Okular selected the text but did not change the primary selection, which only selecting with the mouse did. Speak-selection would then read whatever was selected before, without saying so. In stage 3, check select-all and Shift with the arrow keys in each application of the 9.4 checklist. If they do not set it, the notification must make clear what was read (FR-03), and the clipboard fallback (Ctrl+C, then speak-clipboard) covers it.

#### 7.1.3 Windows

Clipboard reading is simple (`QClipboard` works without focus on Windows). Selection reading has no system equivalent of PRIMARY, so it uses two strategies in order:

1. **UI Automation.**
   - Get the focused element with `IUIAutomation::GetFocusedElement`.
   - Query `TextPattern` (or `TextPattern2`), call `GetSelection`, then `GetText(-1)` on each range.
   - Works in Notepad, Word, WordPad, many WinUI apps, and in Chromium- and Firefox-based browsers once they have enabled UIA support.
   - Run on a dedicated COM thread (MTA). Set the `IUIAutomation2` connection and transaction timeouts to 300 ms, so a hung target app can't stall Legilo.
   - If a call still hasn't returned after the timeout, abandon that thread and use a new one for the next capture, so later captures don't queue behind a stuck one.
2. **Simulated copy (fallback).**
   - Never used in executables listed in `windows/excludedApps` (Appendix A) or in console windows (below).
   - Snapshot the clipboard. Save the text formats (`CF_UNICODETEXT`, HTML, RTF). Arbitrary formats and delayed rendering cannot be saved fully; this is a documented limitation.
   - Wait until the user has released the shortcut's modifier keys (poll `GetAsyncKeyState`, maximum 500 ms). Otherwise the target sees the shortcut's modifiers plus C (for example Win+Alt+C) instead of Ctrl+C.
   - Record `GetClipboardSequenceNumber()`. Send Ctrl+C with `SendInput`.
   - Wait for the sequence number to change (poll every 10 ms, maximum 400 ms). Read the text.
   - Restore the snapshot.
   - **Never simulate Ctrl+C in console windows.** In a terminal, Ctrl+C interrupts the running program. Detect console windows by class name (`ConsoleWindowClass`, `CASCADIA_HOSTING_WINDOW_CLASS`) and skip the fallback there, with a notification saying to copy first and use "speak clipboard".
   - The copied text will appear in the Windows clipboard history (Win+V). That can't be prevented, because the source app writes it. Document this.

`ownerHint` is filled from `GetClipboardOwner`, then `GetWindowThreadProcessId`, then the process image name. The PDF detector treats known PDF readers (`AcroRd32.exe`, `Acrobat.exe`, `SumatraPDF.exe`, `FoxitPDFReader.exe`) as a strong positive signal.

### 7.2 Text processing pipeline

#### 7.2.1 Pipeline steps

```
raw text
  -> 1. Normalise        (always-safe character fixes)
  -> 2. Detect           (PDF-likeness score, unless the mode is Always or Never)
  -> 3. Reflow           (PDF mode: page numbers, repeated headers, hyphenation, line joining;
                          word-list-confirmed de-hyphenation in all modes)
  -> 4. Whitespace       (collapse runs of spaces, trim, paragraph separators)
  -> 5. Segment          (sentences with offsets into the cleaned text)
  -> Document            (cleaned text, as displayed)

at synthesis, per segment:
  -> 6. Speech rules     (URLs, emails, citations, symbols, as configured)
  -> spoken text
```

Every step is a pure function over small value types, configured by a `CleanupSettings` value object:

- Normalise returns the text plus the flags it recorded (such as whether ligatures were present).
- Detect takes the normalised text, those flags and the capture metadata (MIME types, owner hint), and returns a `DetectionResult`.
- Reflow, whitespace and speech rules are `QString -> QString`. Segment returns `Segments`.

This keeps each rule independently testable and lets the preview re-run the whole pipeline cheaply when a setting changes.

Speech rules run per segment, at synthesis time, so the reader shows the text as it appears in the source (URLs, citations, symbols) while the voice reads the spoken form (7.2.8).

All `QRegularExpression` patterns are built with `QRegularExpression::UseUnicodePropertiesOption`. Without it, `\w` and `\b` match ASCII only, unlike Python 3.

#### 7.2.2 Always-safe rules (pipeline step 1)

These run on all text and should never damage normal prose:

- Replace ligatures U+FB00 to U+FB06 (ff, fi, fl, ffi, ffl, long st, st) with their letter sequences.
- Map full-width ASCII forms (U+FF01 to U+FF5E) to ASCII.
- Remove soft hyphens (U+00AD) and zero-width characters (U+200B, U+200C, U+200D, U+FEFF). Use named constants in code, never invisible literals.
- Map non-breaking and other Unicode spaces to a normal space.
- Normalise line endings to `\n`.
- **Do not apply full NFKC normalisation.** It also turns superscripts and fractions into plain digits ("x" + U+00B2 superscript two becomes "x2"; U+00BD one half becomes "1" + U+2044 fraction slash + "2"), which then read wrongly. Normalisation leaves those characters alone. A speech rule says them (7.2.5).

#### 7.2.3 PDF detection (pipeline step 2)

The selection and clipboard do not reliably say where text came from, so detection is based on the shape of the text, plus weak hints from MIME types and, on Windows, the owning process.

Features are computed on the normalised text, split into lines. Blank lines are ignored, because no PDF reader's copy marks paragraphs with them reliably (12, stage 2 results). A line's successor is the next non-empty line, and every non-empty line but the last is a body line. A body line **wraps** when it holds at least three words of two or more letters, ends without terminal punctuation (`.!?:;` plus closing quotes or brackets), and the next line starts with a lowercase letter. The word count keeps equations and table cells, copied a symbol or a cell to a line, from looking like prose running on. The first draft also counted a leading digit or opening bracket, but in the corpus that marked logs, tables, chat and checklists far more often than prose.

| Feature | Definition | Weight |
|---|---|---|
| `wrap` | The longest run of body lines that wrap, as a fraction of eight lines (at most 1). A run, not a share of all lines, so that an address block or a table above wrapped prose does not hide it, and a few short lines of chat that happen to run on count for little | 4.89 |
| `midline` | Fraction of lines in which one sentence ends and another begins: a lowercase word of two or more letters, or a closing bracket, then `.`, `!` or `?`, an optional closing quote or bracket, whitespace and a capital. "Dr. Smith" and initials do not count | 1.30 |
| `hyphen` | 1 if a line ends with a letter followed by `-` and the next line starts with a lowercase letter, or a line holds "word- word", Chromium's form of a hyphenated break | 1.51 |
| `ligature` | 1 if the raw text contained ligature characters (recorded during normalisation) | 0 |
| `pagenum` | 1 if any line matches the page-number rule (section 7.2.4) | 0.41 |
| `owner` | 1 if `ownerHint` is a known PDF reader (Windows only) | 0.22 |
| `list` | Fraction of lines that start with a list marker (`-`, `*`, a bullet character, `1.`, `a)`, or a clause number such as `2.1` before a capital) | 0.29 |

Negative signals subtract from the score:

| Signal | Definition | Weight |
|---|---|---|
| `symbols` | Fraction of lines in which at least 5% of the characters are symbols: brackets of every kind, `=`, `;`, `$`, `#`, slashes, `&`, `*`, `_`, `~`, `^`, `@`, `%`, the vertical bar and the backtick. It marks code, shell sessions, markup and tables | -1.36 |
| `html` | 1 if the capture offered `text/html` (rich-text sources are rarely raw PDF text) | -0.11 |

```
if fewer than 3 non-empty lines: not PDF (nothing to reflow anyway)
score = sum(weight * feature)
isPdf = score >= 1
```

The weights are fitted by logistic regression on the regression corpus's tuning set (section 9.2) with `tests/corpus/tools/tune_detector.py`. The tool places the threshold for the highest recall at a tuning precision of at least 0.99, and scales the weights so the threshold is 1, which keeps one fit comparable with the next.

- `owner` and `html` cannot be fitted, because no corpus sample carries those hints. They keep the first draft's weights (0.10 and -0.05 against a threshold of 0.45), scaled the same way. No sample has ligatures either, so the fit leaves `ligature` at 0.
- The first draft's `uniform` (the share of body lines at least 75% as long as the 90th-percentile line) fitted to about zero: Chromium copies a paragraph as one long line, and table rows are as uniform as justified prose. It was dropped.
- The first draft's `code` signal became the broader `symbols`.
- `list` was a negative signal in the first draft. It fits slightly positive, because numbered clauses and recipe steps wrap in PDF text as prose does, and reflow keeps the break before each list item anyway (7.2.4, rule 5).
- The weights were refitted once the corpus grew, with the features unchanged. The results, and the known weakness, are in section 12 (stage 2 results).

The minimum of 3 lines means a two-line selection is never treated as PDF text. The most common hyphenation break in such a selection is still fixed, because confirmed de-hyphenation runs in all modes (7.2.4, rule 3). Detection exposes the score and each feature value, so the preview can explain its decision ("Detected as PDF text: wrapped lines 100%, sentences inside lines 40%").

Modes, set in the settings and the preview:

- **Auto** (default): reflow if `isPdf`.
- **Always**: reflow regardless.
- **Never**: no reflow.

The "read as PDF text" shortcut forces Always for one run.

#### 7.2.4 Reflow rules (pipeline step 3)

All rules apply in PDF mode only, except the confirmed join in rule 3, which applies in all modes. Applied in this order:

1. **Page-number lines.** Remove a line if, ignoring surrounding whitespace, it consists only of:
   - an optional leading dash (`-`, U+2013, U+2014), then
   - an optional "Page " or "p. " (case-insensitive), then
   - a page number, then
   - an optional " of N", then
   - an optional trailing dash.

   The page number is either:
   - an Arabic number of 1 to 4 digits, or
   - a **valid** lowercase Roman numeral, checked with `^(?=[mdclxvi])m{0,3}(cm|cd|d?c{0,3})(xc|xl|l?x{0,3})(ix|iv|v?i{0,3})$`, or
   - an uppercase Roman numeral, but only when preceded by "Page".

   This fixes the script's pattern, which deleted lines such as "I", "ill", "civil" or "vivid".

   Skip the rule for a run of two or more consecutive number-only lines. That is a table column, not a page number.
2. **Repeated running headers and footers.** Count each line after replacing digits with `#` and trimming. Remove a line if:
   - it is short (under 60 characters),
   - it contains at least three letters,
   - it occurs at least twice in the text, and
   - it is not a sentence (does not end with terminal punctuation).

   This catches "Journal of Something, Vol. 12" repeated across a multi-page selection. The letter requirement keeps table rows such as "12.5" and "13.7", which both become "##.#". Toggle, default on.
3. **De-hyphenation across line breaks.** First, in PDF mode, a blank line inside a sentence is dropped: one after a line that ends without terminal punctuation and before a line that starts with a lowercase letter. Firefox's copies put one between many lines of a paragraph, often between the halves of a hyphenated word (12, stage 2 results). Then, for `(\w+)-[ \t]*\n[ \t]*(\w+)`:
   - Keep the hyphen if both halves are words in the word list and the joined form is not (for example, "well-known" stays).
   - **Confirmed join (all modes):** join the halves without the hyphen if the joined form is in the word list and at least one half is not (for example, "re-" + "sults").
   - **PDF mode only:** in every other case, join the halves without the hyphen. With no word list available, join.
   - In PDF mode the joined word ends the first line, and the rest of the second line stays on a line of its own. Deleting the line break would merge two lines into one twice the usual width, and rule 5 judges paragraph ends by that width.
   - **Inside a line (PDF mode only):** "word- word", Chromium's form of a hyphenated break, is changed only where the word list settles it: joined without the hyphen for a confirmed join, and with it where both halves are words and the joined form is not. Anything else is left as it is, because a hyphen and a space can be meant ("pre- and post-tests").
4. **Dash at line end.** For a line ending in an em or en dash (U+2014, U+2013), join to the next line without inserting a space.
5. **Line joining.**
   - Blank lines that rule 3 left are paragraph breaks.
   - Within a paragraph, a line break becomes a space, **except** in three cases, where it becomes a paragraph break:
     - before a line that starts with a list marker (as in 7.2.3, clause numbers included);
     - after a line under 80% of the 90th-percentile line length that ends with terminal punctuation: the last line of a paragraph, or a heading. Okular's and Chromium's copies have no blank lines, so this is how their paragraphs come back;
     - after a line under 50% of that length that has letters and no terminal punctuation, when the next line starts with a capital: a heading without a full stop ("Method", "1. Introduction").
   - The exceptions preserve lists and headings that the script merged into the previous paragraph. The 80% was tuned on the clean-up pairs (9.2), for the most paragraphs recovered exactly; the pairs are six documents typeset for the corpus, which is a reason to watch it in real use.

#### 7.2.5 Speech rules (per segment, at synthesis)

These rules change what is spoken, not what is displayed. They run on each segment's text just before it is synthesised (7.2.1), and the preview shows their result (7.12).

- **URLs** (`https?://...` or `www....`). Setting: speak as "a link to example.com" (default), speak in full, or remove.
  - Trailing punctuation is excluded from the URL, but a closing bracket is kept if the URL contains an unmatched opening bracket (for example, Wikipedia links such as `.../Foo_(bar)`).
  - The script stripped these.
- **Email addresses.** Setting: speak as "an email address at example.com" (default) or in full.
- **Numeric citation markers** `[12]`, `[3, 4]`, `[5-7]`, `[5` + U+2013 + `7]`. Removed in PDF mode by default, and optionally in all text. In ordinary text they are left alone by default, because square-bracketed numbers there are often meaningful.
- **Author-year citations** such as `(Smith et al., 2020)` or `(Smith and Jones 2019; Lee 2021)`. Optional, default off. Matches a bracketed group of one or more entries separated by `;`. Each entry is capitalised names, an optional "et al.", and a four-digit year with an optional letter (2020a).
- **Superscripts and fractions.** A small table: U+00B2 to " squared", U+00B3 to " cubed", U+00BD to " a half", U+00BC to " a quarter", U+00BE to " three quarters". In stage 2, drop any entry that espeak-ng is shown by test to read correctly already.

#### 7.2.6 Fixes relative to `speak-tts` and `speak-selection`

| # | Issue in the scripts | Fix in Legilo |
|---|---|---|
| 1 | The page-number pattern `[ivxlc]{1,6}` with case-insensitive matching deleted real words on their own line ("I", "ill", "civil", "vivid") | Valid Roman numerals only, lowercase unless preceded by "Page", and only in PDF mode (7.2.4) |
| 2 | Soft-hyphen removal used an invisible literal character that editors can silently strip | Named constant for U+00AD (7.2.2) |
| 3 | Selection silently fell back to the clipboard, which could read stale text | Separate selection and clipboard actions; fallback is optional, and the notification says which source was read (FR-03) |
| 4 | De-hyphenation depended on `/usr/share/dict/words`, which is not installed by default on Debian and doesn't exist on Windows | Bundled word list as a compiled-in resource (7.2.4) |
| 5 | NFKC normalisation mangled superscripts and fractions | Targeted normalisation (7.2.2) and a speech rule for superscripts and fractions (7.2.5) |
| 6 | A closing bracket was always stripped from the end of URLs | Bracket-balance check (7.2.5) |
| 7 | Line joining merged list items and headings into the previous paragraph | List and heading exceptions (7.2.4) |
| 8 | All clean-up rules applied to all text, so ordinary text with deliberate line breaks (poems, addresses, lists) was flattened | Reflow only for detected PDF text, with Auto/Always/Never (7.2.3) |
| 9 | Citation removal applied to all text | PDF mode by default (7.2.5) |
| 10 | Pause used SIGSTOP on the synthesis and playback processes, which stalls the socket and can glitch on resume | Real pause in the audio device; synthesis continues ahead into the buffer (7.4) |
| 11 | Race between "is it running?" and "start" when the shortcut was pressed twice quickly | Single instance with IPC; all commands go through one state machine (7.5, 7.8) |
| 12 | Running headers and footers were spoken | Repeated-line rule (7.2.4) |
| 13 | A dash at the end of a line was joined with a space | Dash rule (7.2.4) |
| 14 | Regular expressions relied on Python's Unicode-aware `\w`; a straight port to Qt would silently become ASCII-only | `UseUnicodePropertiesOption` everywhere (7.2.1) |

#### 7.2.7 Segmentation

Segments are what get synthesised, highlighted and navigated, so their boundaries matter more than in the script, where Piper split internally. Legilo splits text itself and synthesises each segment separately. That gives exact audio boundaries for highlighting and seeking.

Rules (English):

1. A paragraph break always ends a segment.
2. A boundary candidate is `.`, `!` or `?`, optionally followed by closing quotes or brackets, then whitespace, then an uppercase letter, digit, or opening quote or bracket.
3. A candidate is rejected when the preceding token:
   - is an abbreviation that never ends a sentence: Mr, Mrs, Ms, Dr, Prof, St, vs, e.g, i.e, cf, al (as in "et al."), approx, Jan to Dec, plus the configurable list (`cleanup/abbreviations`), or
   - is an abbreviation that is only one before a number, and the next token is a digit: No, Nos, Fig, Figs, Eq, Eqs, Vol, p, pp ("No. 5" is not a boundary; "The answer was No. Then" is), or
   - is a single capital letter (initials such as "J. R. R. Tolkien"), or
   - forms part of a decimal number.

   Abbreviations that often end a sentence (etc, Inc, Ltd, Co, Corp, Jr, Sr) do not block a boundary. A following uppercase letter is treated as a new sentence.
4. An ellipsis (`...` or U+2026) followed by a lowercase letter is not a boundary.
5. URLs and email addresses (as matched in 7.2.5) are single tokens: no boundary falls inside them. This matters because speech rules run after segmentation.
6. Maximum segment length is 300 characters (configurable). Longer sentences are split at the separator nearest the middle: first `;`, then `:`, then a dash, then a comma, then any whitespace. Splitting repeats until every part is within the limit. Short segments give better prosody than one long synthesis. First-audio latency is handled separately by the short first chunk (7.5).
7. A segment shorter than 3 characters merges into the previous segment in the same paragraph. If it starts a paragraph, it merges into the next one. A paragraph shorter than 3 characters in total stays as its own segment. Merging never crosses a paragraph break (rule 1).
   - A segment of a single word merges the same way, whatever its length. Even in Piper's own runtime, cori babbles on some isolated short words: about one call in five for "Eight." and nearly every call for "Yes." (12). The short first chunk (7.5) never splits off a single word either.

Qt's `QTextBoundaryFinder` (Unicode sentence boundaries) is not sufficient on its own: it breaks after "Mr." and "Dr.". The custom rules replace it entirely (`core/text/Segmenter`, stage 2); they cover every case in the tests without it.

#### 7.2.8 Data model

```cpp
struct Segment {
    qsizetype start;        // offset into Document::cleaned, inclusive
    qsizetype end;          // exclusive
    int paragraph;          // paragraph index, for paragraph navigation
};

struct DetectionResult {
    double score;
    bool isPdf;
    QMap<QString, double> features;   // for the preview's explanation
};

struct Document {
    QString id;             // stable key for position storage (7.11.5)
    QString title;          // file name, or the first words of pasted text
    CaptureSource source;
    QString original;       // after normalisation, before everything else
    QString cleaned;        // what is displayed; speech rules apply per segment (7.2.5)
    QList<Segment> segments;
    DetectionResult detection;
    CleanupSettings settingsUsed;
};
```

The reader displays `cleaned`, so highlighting needs no offset mapping back to the original.

Speech rules are applied to each segment's text only when it is synthesised. So the reader shows a URL, a citation or "x" + U+00B2 as written, while the voice says "a link to example.com" or "x squared".

The preview shows the relationship between `original` and the spoken form through a diff (7.12), instead of tracking every edit through every rule. That keeps the rules simple. Where a position has to be carried from one text to another (resume, re-segmentation), it is mapped through a diff with diff-match-patch's `diff_xIndex` (7.11.5, 7.12).

### 7.3 TTS engine

#### 7.3.1 sherpa-onnx usage

- Use the sherpa-onnx C++ API wrapper (`sherpa-onnx/c-api/cxx-api.h`), which wraps the stable C API. Pin one 2.x release and upgrade deliberately; the API still changes.
- One `OfflineTts` instance per loaded voice. Configuration:
  - **Piper (VITS):** `model.vits.model`, `model.vits.tokens`.
  - **Kokoro:** `model.kokoro.model`, `model.kokoro.voices`, `model.kokoro.tokens`.
  - `data_dir` is ignored since sherpa-onnx 2.0. The model archives still contain `espeak-ng-data`; Legilo doesn't use it (see "Phonemisation" below).
  - `model.num_threads`: a setting, defaulting to half the logical cores, clamped to the range 1 to 4.
  - `max_num_sentences = 1`, because Legilo segments the text itself.
- Synthesis: `Generate` with a generation config holding the phonemes (below), speaker ID and speed, plus a callback. It returns float samples plus the sample rate. If the callback returns 0, synthesis is cancelled.
  - The callback runs only between sherpa-onnx's internal sentence batches. A single `Generate` call on one sentence can't be interrupted part-way.
  - So stop and seek silence playback at once (7.4) and discard the in-flight result by generation. The worker is free again within one chunk's synthesis time, which the short first chunk (7.5) keeps small.
- Thread safety: checked in stage 1. Concurrent `Generate` calls on one `OfflineTts` instance and on two produced sound speech every time (`tests/integration/tst_synthesis.cpp`), so there is no process-wide lock. A short test is evidence, not proof: if the export loop (7.13) shows problems, serialise calls with one lock and have export give way to playback between segments.
- Speed: passed per call (1.0 is normal; range 0.5 to 2.5, step 0.1). A change applies from the next segment that has not yet been synthesised. The controller discards look-ahead audio that hasn't been played yet and re-synthesises it, so a change is heard within one segment.
  - Speed does not scale duration in proportion. A Piper voice at 2.0 takes about 0.66 of the time, and at 0.5 about 1.6 times, because each sound keeps at least one frame. The slider's labels are nominal.
- Synthesis is not deterministic: each call draws fresh noise, durations included, so the same sentence varies by about 7% in length from call to call. Tests cannot compare audio exactly.

Field and function names above, and the callback granularity, match recent sherpa-onnx releases. Verify them against the pinned version in stage 1.

**Phonemisation.** sherpa-onnx 2.0 removed espeak-ng and piper-phonemize, so that sherpa-onnx contains no GPL code. For Piper and Kokoro voices the caller now supplies phonemes, as `phoneme_codepoints` in the generation config instead of `text`. Legilo's `Phonemiser` (`core/tts`) does this:

- It links espeak-ng: Debian's `libespeak-ng` (1.52) on Linux, with its data from the `espeak-ng-data` package; on Windows, espeak-ng built from source, with its data bundled.
- It phonemises one segment at a time, clause by clause, with `espeak_TextToPhonemesWithTerminator` in IPA mode, and appends each clause's punctuation from the terminator. This is what piper-phonemize does, and the Piper voices were trained on its output.
- The espeak-ng voice comes from the model's metadata, which sherpa-onnx returns from `OfflineTts::Lang()`: `en` for British English (`en-gb` is not an espeak-ng voice name) and `en-us` for American. A speaker's `espeakVoice` in the voice manifest overrides it (7.3.2), because Kokoro's metadata says `en-us` for all its speakers, the British ones included.
- It uses the system's espeak-ng data, not the data piper-phonemize ships from its own espeak-ng fork. Checked in stage 1: with the fork's data the `Phonemiser` matches every golden fixture, and with Debian's 1.52 data it matches every `en` fixture. For `en-us`, 1.52 gives an open o before r ("four", "record") where the fork gives a close-mid o; the unit test records that one difference. Bundling the fork's data is the fallback if the listening check finds it audible.
- espeak-ng keeps global state, so there is one process-wide `Phonemiser`, with calls serialised by a mutex. Phonemising takes far less time than synthesis, so the lock is not a bottleneck.
- It is written test-first against golden fixtures. The phoneme strings are generated once with piper-phonemize and committed, covering sentences, punctuation and clause terminators, numbers, abbreviations and non-ASCII text.
- Kokoro takes the same phoneme stream as Piper (checked in stage 1). Kokoro v0.19 was trained on espeak-ng IPA.
- Owning the phonemiser also makes per-word pronunciation fixes possible later (15).

#### 7.3.2 Voices

```cpp
struct Voice {
    QString id;             // e.g. "piper-en_GB-cori-medium" or "kokoro-en-v0_19:bf_emma"
    QString modelId;        // the installed model; its speakers share it
    QString engine;         // "piper" | "kokoro"
    QString displayName;
    QString language;       // "en-GB", "en-US"
    int speakerId = 0;      // multi-speaker models
    QString espeakVoice;    // overrides the model's espeak-ng voice; usually empty
    QString directory;      // installed location
    QString modelPath, tokensPath, voicesPath;  // voicesPath for Kokoro only
    QString licence;        // SPDX ID or short name; "unknown" if not recorded
    QUrl licenceUrl;
};
```

- The sample rate is not part of `Voice`: only the loaded model knows it reliably, so `TtsEngine::sampleRate()` reports it (22050 for Piper medium, 24000 for Kokoro).
- `TtsEngine` is the interface: `sampleRate()` and `synthesise(text, speed)`, which returns mono float samples, no samples for text with nothing to say, or nothing on failure. `SherpaTtsEngine::load(voice, threads)` creates one per loaded voice.

- `VoiceRegistry` scans the voices directory at start-up and after installs.
  - Directory: `QStandardPaths::AppLocalDataLocation` + `/voices/`, which is `~/.local/share/legilo/voices/` on Linux and `%LOCALAPPDATA%\legilo\voices\` on Windows.
  - Not `AppDataLocation`: on Windows that is the roaming profile, and voices are hundreds of megabytes.
  - Leave the organisation name empty, or the path gains a second `legilo/` level.
  - Each voice directory holds the model files plus a `legilo-voice.json` manifest written at install time. Format 1, documented in `VoiceRegistry.h`, holds the model's ID, engine, name, files (relative to its directory), and optionally its language, licence and speakers. A speaker may carry `espeakVoice`; the catalogue and installer set it to `en` for Kokoro's British speakers (`bf_*`, `bm_*`) in stage 4.
  - Each speaker of a multi-speaker model is its own voice, with the ID `model:speaker`.
  - A manifest that is invalid, names a missing file or one outside its directory, or repeats an earlier ID is skipped with a warning, so one broken install never hides the rest.
  - The registry ignores directories without a manifest, and the installer's temporary directories (7.10).
- **Installing:** one installer component (`ModelInstaller`) serves both `legilo --install-voice ARCHIVE` (stage 1, FR-28) and the model manager (stage 4).
  - It verifies the archive's SHA-256 against the catalogue when the archive is listed there. An unlisted archive installs with its licence marked unknown.
  - It writes the manifest.
- **Keeping voices loaded:** the default voice loads in the background at start-up, and stays loaded (the equivalent of the script's warm server).
  - Other voices load on first use and are kept in a least-recently-used (LRU) cache of up to 2 models (setting), to bound memory.
  - Loaded voices are held by `std::shared_ptr`, so evicting one from the cache never frees an instance that is still in use.
  - Unloading happens on the synthesis thread.
  - Optional: unload all voices after a period of no playback (`voice/unloadAfterIdleMin`, default 0 for never). A resident tray app holding a few hundred megabytes all day is noticeable. The next speak then pays the load time.
- **Slow voices:** Kokoro and Piper "high" voices run at about real time on a 4-core laptop CPU (stage 0 results, 12). That is too slow to stay ahead of playback, and first audio takes seconds.
  - The catalogue marks them with `needsFastCpu`, and the model manager says so beside them.
  - While playing, the controller compares synthesis time with audio length. If playback runs dry twice in one document, it notifies once, suggesting a "medium" voice.
  - Use fp32 Kokoro, not int8: on CPUs without VNNI instructions, int8 is slower.
- **Existing Piper voices:** the voices in `~/.local/share/piper-voices` (`.onnx` plus `.onnx.json`) cannot be used directly, because sherpa-onnx needs extra metadata in the ONNX file.
  - sherpa-onnx publishes pre-converted versions of the Piper voices in its TTS model releases, so the model manager downloads those instead.
  - Check the catalogue for the specific voices you use, such as `en_GB-alba-medium`. Check its licence too: alba is fine-tuned from lessac (section 11).
  - Importing your own `.onnx` files would need the conversion step reimplemented in C++. Deferred.

### 7.4 Audio output

- miniaudio playback device, 32-bit float, mono.
  - The sample rate is set to the voice's rate, and miniaudio converts to the device's native rate.
  - Changing to a voice with a different sample rate reinitialises the device between documents.
- **`AudioQueue`:** a lock-free single-producer, single-consumer ring buffer of samples, plus a small companion queue (also single-producer, single-consumer) of `(generation, segmentIndex, firstFrame, frameCount)` records.
  - The synthesis worker produces; the audio callback consumes.
  - Capacity: 60 s of audio.
- **Look-ahead:** synthesise until either 3 segments or 15 s of audio are queued ahead of the play position, whichever comes first.
- **Position:** the callback updates `std::atomic<int> currentSegment` and `std::atomic<qint64> framesPlayed` as it crosses segment records. The GUI polls these.
- **Flushing (stop, seek, speed or voice change):** only the worker writes queued data, and only the callback reads or discards it. The GUI never touches the buffer.
  - The controller requests a flush with `keepThroughSegment`, which starts a new generation.
    - For stop and seek, `keepThroughSegment` is -1, so everything goes.
    - For a speed or voice change, it is the segment currently playing.
  - Requests reach the callback through their own small single-producer, single-consumer queue, and it applies each in order. Applying only the latest request is not enough: after a seek, a speed change that arrives before the callback runs (as it can while paused) would keep audio from before the seek whose segment number happens to be lower.
  - For each request, the callback marks dead every queued record older than the request's generation whose segment is after `keepThroughSegment`. Discarding a record skips its frames in the ring.
  - The worker tags every chunk with the generation it was produced for, and stops work on a generation that has moved on. A chunk it still pushes for an old generation after a flush was applied is discarded, so stale audio is never played after a seek.
  - If the callback falls 64 requests behind, which happens only when it has stopped running, the request fails and the controller must not rely on it.
- **Pause:**
  - Stop the device (`ma_device_stop`) and keep the queue, so resuming has no gap or lost audio.
  - Optional setting: "rewind to start of sentence on resume" (default off).
- **Device changes:**
  - Losing the device that is playing (headphones unplugged, Bluetooth disconnected) **pauses** playback. It does not carry on through another device. Reading private text aloud on the laptop speakers is the wrong default, and phones behave the same way.
  - Resume plays on the new default device, from the current segment start.
  - Use miniaudio's device notification callback (rerouted and stopped notifications). It fires on a miniaudio thread, so forward it to the GUI thread. Implemented: a reroute, or a stop Legilo did not ask for, stops the output and emits `AudioOutput::deviceLost()`. Checked by hand in stage 1: unplugging wired headphones under PipeWire is reported as a reroute, and playback stops (12). Bluetooth and Windows remain for the manual checklist (9.4).
  - Check first how much miniaudio already does. Its WASAPI and PulseAudio backends follow the default device automatically, so the work may reduce to detecting the change and pausing, with no re-initialisation of our own.
- **Optional leading silence trim and inter-segment gap:**
  - Trim leading silence, so segments join naturally.
  - Insert a configurable gap (default 120 ms) between segments, and 350 ms between paragraphs.

### 7.5 Playback controller

States and transitions:

```
          play(doc, i)              first audio queued
  Idle -------------------> Starting ------------------> Playing
   ^                          |  stop                    |  ^   pause
   |                          v                          |  |  ------> Paused
   +------------------------ Stopping <------------------+  +-------- resume
   |   queue drained / cancelled           stop / end of document
   +-------------------------------------------------------------------------
```

Commands (all on the GUI thread):

| Command | Behaviour |
|---|---|
| `play(Document, int startSegment)` | Stops anything current, then starts from `startSegment` |
| `togglePlay()` | Playing becomes Paused; Paused becomes Playing; Idle replays the last document from its last position |
| `pause()` / `resume()` | As described in 7.4 |
| `stop()` | Cancels synthesis, clears the queue, returns to Idle |
| `seekToSegment(int)` | Generation increment, clear the queue, synthesise from the target |
| `nextSegment()` / `previousSegment()` | Seek by one. "Previous" within the first 1.5 s of a segment goes to the segment before; otherwise to the start of the current one, like music players. |
| `nextParagraph()` / `previousParagraph()` | Seek to the first segment of the next or previous paragraph |
| `setSpeed(double)` / `setVoice(QString)` | Applies from the next unsynthesised segment (7.3.1). The faster and slower actions change the speed by 0.1 for current and later playback, and save it as `voice/speed` |

Signals: `stateChanged(State)`, `segmentChanged(int index)`, `documentFinished()`, `error(QString)`.

**Short first chunk.** A full 300-character segment is 15 to 20 s of speech. Synthesis returns nothing until the whole call finishes, so synthesising it in one go would miss NFR-01.

- When playback starts or seeks, the starting segment is synthesised in two chunks:
  - The first chunk runs up to the first separator (`;`, `:`, a dash, a comma, then any whitespace) after about 80 characters (`voice/firstChunkChars`).
  - The second chunk is the rest.
- Both chunks carry the same segment index, so highlighting and navigation are unchanged.
- Later segments are synthesised whole while the earlier ones play.

The shortcut toggle rule (FR-04): if a speak-selection or speak-clipboard shortcut fires while the state is Starting, Playing or Paused, the setting `behaviour/speakWhileSpeaking` decides:
- `stop` (default, matching the script): stop playback.
- `restart`: capture the new text and play it.

### 7.6 Global shortcuts

#### 7.6.1 Interface

```cpp
class GlobalShortcuts : public QObject {
    Q_OBJECT
public:
    // Registers an action with its default. The user's saved shortcut wins if present.
    virtual void registerAction(const QString &id, const QString &label,
                                const QKeySequence &defaultSequence) = 0;
    virtual bool setShortcut(const QString &id, const QKeySequence &sequence) = 0;
    virtual QKeySequence shortcut(const QString &id) const = 0;
    virtual bool isAvailable(const QKeySequence &sequence) const = 0;
signals:
    void activated(const QString &id);
    void shortcutChanged(const QString &id, const QKeySequence &sequence);
};
```

#### 7.6.2 Actions and defaults

| ID | Label | Default (Linux) | Default (Windows) |
|---|---|---|---|
| `speak-selection` | Speak selection | Meta+Alt+S | Win+Alt+S |
| `speak-clipboard` | Speak clipboard | Meta+Alt+C | Win+Alt+C |
| `speak-pdf-text` | Speak selection as PDF text | none | none |
| `pause-resume` | Pause or resume | Meta+Alt+P | Win+Alt+P |
| `stop` | Stop | Meta+Alt+X | Win+Alt+X |
| `faster` | Faster | none | none |
| `slower` | Slower | none | none |
| `show-reader` | Show reader | Meta+Alt+R | none |

Defaults are checked against existing bindings at first run. A default that is already taken is left unassigned, and the settings dialog shows "not set".

Windows defaults:

- **No Ctrl+Alt combinations.** Windows sends AltGr as Ctrl+Alt, so a Ctrl+Alt+S hotkey takes over AltGr+S. On many European layouts that key types a letter (on a Polish layout, AltGr+S, C and X type U+015B, U+0107 and U+017A: s, c and z with an acute accent).
  - `RegisterHotKey` succeeds anyway, so the first-run check can't detect the clash.
  - The Windows defaults therefore mirror the Linux ones, with Win in place of Meta.
- `show-reader` has no Windows default, because Win+Alt+R belongs to the Xbox Game Bar.
- Check each default against Windows' reserved combinations in stage 5.

#### 7.6.3 Linux: KGlobalAccel

- **Registration:**
  - One `QAction` per action ID, with `objectName` set to the ID. The object name is the stable key stored by kglobalaccel.
  - Register with `KGlobalAccel::setGlobalShortcut(action, default)` at start-up. That call uses autoloading, so the user's saved choice wins.
- **Changes from the in-app settings dialog:** call `KGlobalAccel::self()->setShortcut(action, {seq}, KGlobalAccel::NoAutoloading)`.
- **Changes made in System Settings:** these arrive through `KGlobalAccel::globalShortcutChanged`, which is forwarded as `shortcutChanged`.
- **Availability:** checked with `KGlobalAccel::isGlobalShortcutAvailable()`. If a shortcut is taken, show who owns it and offer to choose another (taking over another component's shortcut is not offered in version 1).
- **System Settings grouping:** set `QGuiApplication::setDesktopFileName("io.github.example.legilo")` and ship a matching `.desktop` file, so the shortcuts appear under the app's name and icon in System Settings > Keyboard > Shortcuts.

#### 7.6.4 Windows: RegisterHotKey

- **Registration:**
  - A hidden message-only window (`HWND_MESSAGE`) owned by the GUI thread.
  - `RegisterHotKey` with `MOD_NOREPEAT` for each assigned shortcut.
  - `WM_HOTKEY` is caught through a `QAbstractNativeEventFilter`.
- **Key mapping:** a small table converts a `QKeySequence` (single chord only) to virtual-key and modifier flags.
- **Conflicts:** if `RegisterHotKey` fails, another app owns that combination. The settings dialog shows "already in use by another application", and the shortcut stays unassigned.
- **Storage:** shortcuts are stored in Legilo's own settings. There is no system UI for app hotkeys on Windows.

#### 7.6.5 In-app editor

- Use `QKeySequenceEdit`, limited to one chord, with clear and reset-to-default buttons.
- Validation:
  - Require at least one modifier (Ctrl, Alt or Meta/Win); Shift alone is not enough.
  - Check `isAvailable()` before applying.
  - On Windows, warn before accepting a Ctrl+Alt or Ctrl+Alt+Shift combination: it is AltGr (or AltGr+Shift) on many keyboard layouts (7.6.2).

### 7.7 Tray, notifications, media controls and autostart

- **Tray icon:** `QSystemTrayIcon`, which uses StatusNotifierItem on Plasma.
  - The icon changes with state (idle, speaking, paused).
  - The tooltip shows the current state and the first words of the current sentence.
  - Left click toggles the reader window; middle click toggles pause.
- **Notifications:** `QSystemTrayIcon::showMessage` in version 1, which works on both platforms.
  - Linux can move to KNotifications later for richer notifications.
  - The started, paused and source-used notifications can each be turned off. Error notifications are always shown.
- **Media controls (FR-29, stage 6):** behind a `MediaControls` platform interface, fed by the playback controller's signals.
  - Linux: an MPRIS player (`org.mpris.MediaPlayer2.legilo`) over Qt DBus. The Plasma media applet, the lock screen and headset buttons can then play, pause, stop and step by sentence (Next/Previous).
  - Windows: System Media Transport Controls, for the media flyout and hardware media keys.
  - Metadata: the document title and the current sentence's first words.
- **Autostart:**
  - Linux: write or remove `~/.config/autostart/legilo.desktop`, with `Exec=legilo --tray`.
  - Windows: set or remove a value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
  - Both sit behind the `Autostart` platform interface.

### 7.8 Single instance, IPC and command line

- **One binary, `legilo`.** On start:
  1. Try to connect to the instance's local server. If a running instance answers, send the command and exit with the reply's status code.
  2. Otherwise take a `QLockFile` (`legilo.lock` next to the server address), waiting up to 2 s.
  3. With the lock held, try to connect once more, because another process may have just become the instance. If one answers, release the lock and continue as in step 1.
  4. Otherwise call `QLocalServer::removeServer` to clear a socket left behind by a crash, listen, and keep the lock for the life of the instance.

  The lock closes the race between two quick launches that both fail to connect (7.2.6, fix 11). Without `removeServer`, a stale socket file would make `listen()` fail after a crash.
- **Server address:**
  - Linux: `QLocalServer` at `$XDG_RUNTIME_DIR/legilo.sock`.
  - Windows: a named pipe, `legilo-<username>-<session ID>`. Named pipes are not per session, so the session ID keeps two sign-ins of the same user apart (for example, Remote Desktop plus the console).
  - Created with `QLocalServer::UserAccessOption`, so only the current user can connect.
- **Protocol:** one JSON object per line.
  - Request: `{"cmd": "speak-selection"}`, `{"cmd": "read-file", "path": "/abs/path.txt"}` or `{"cmd": "speak-text", "text": "..."}`.
  - `speak-text` is refused unless `ipc/allowSpeakText` is on (section 8).
  - Reply: `{"ok": true}` or `{"ok": false, "error": "..."}`.
  - Paths are made absolute by the client before sending.
- **When no instance is running:**
  - `--speak-selection`, `--speak-clipboard`, `--speak-pdf-text`, `--speak-text`, `--read` and `--show` start one. The new process becomes the instance (it stays resident in the tray) and then carries out the command.
  - `--pause`, `--stop`, `--faster` and `--slower` do nothing and exit 0, because nothing is playing.
  - `--clean-only`, `--explain`, `--list-voices` and `--install-voice` run in the calling process and never need an instance.
  - Exit code 3 means an instance was needed but could not be started.
- **Command line:** parsed with `QCommandLineParser`. See Appendix B. It doubles as the fallback for binding actions to desktop shortcuts by hand.
  - `--read` with no files shows the reader, so the menu entry (`Exec=legilo --read %F`, Appendix C) works when launched without files.

### 7.9 Settings

- `QSettings` in INI format on both platforms, for portability and easy backups.
  - Linux: `~/.config/legilo/legilo.conf`.
  - Windows: `%APPDATA%\legilo\legilo.ini`.
- A typed `Settings` facade wraps it: one getter and setter per key, defaults in one place, and a `changed(key)` signal.
- Keys are listed in Appendix A.
- **Settings dialog pages:**
  - General: autostart, notifications, speak-while-speaking behaviour, clipboard fallback.
  - Voice: voice, speed, gaps, threads.
  - Shortcuts.
  - Clean-up: the same panel as the preview (7.12).
  - Models: opens the model manager.

### 7.10 Model manager

- **Catalogue:** `resources/catalogue/voices.json` is compiled into the app. A newer catalogue can be fetched from the project's repository on request, using a "Check for new voices" button; there is no automatic network access.
  - The fetched catalogue must carry a valid Ed25519 signature (minisign format, checked with Monocypher) from a public key compiled into the app. Otherwise it is rejected and the current catalogue stays in use.
  - Downloads are verified against the catalogue's checksums, so those checksums are only as trustworthy as the catalogue itself.
  - Entry format:

```json
{
  "id": "piper-en_GB-cori-medium",
  "engine": "piper",
  "name": "Cori (British English, medium)",
  "language": "en-GB",
  "quality": "medium",
  "needsFastCpu": false,
  "speakers": [{ "id": 0, "name": "Cori" }],
  "url": "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-en_GB-cori-medium.tar.bz2",
  "sha256": "<checksum>",
  "size": 67000000,
  "licence": "LicenseRef-public-domain",
  "licenceUrl": "https://...",
  "sampleUrl": "https://..."
}
```

- **Download:** `QNetworkAccessManager` over HTTPS only, with redirects followed (GitHub release assets redirect).
  - Streamed to a `.part` file, with progress and cancel.
  - Resumable with an HTTP `Range` request if interrupted.
- **Verification:** SHA-256 of the downloaded archive must match the catalogue. A mismatch deletes the file and reports an error.
- **Installation:** `ModelInstaller` (7.3.2, also used by `--install-voice`) reads the tar archive with libarchive into a temporary directory inside the voices directory, named `.install-*` so the registry ignores it.
  - It writes the files itself and accepts only regular files and directories. An absolute path, a `..`, a backslash or colon, a symbolic or hard link, or a device fails the whole install. This is stricter than libarchive's `ARCHIVE_EXTRACT_SECURE_*` flags, which would still create a symbolic link pointing out of the model directory.
  - Archives that unpack to more than 2 GiB are refused. Each model's `espeak-ng-data` directory is skipped (7.3.1).
  - The model ID is the archive's single top-level directory, or its file name, without sherpa-onnx's `vits-` prefix. A model already installed under that ID is left alone.
  - Without a catalogue entry, the manifest's name, language and speakers come from a Piper model's `.onnx.json`, and the licence is "unknown". Kokoro keeps its speaker names inside the ONNX model, so it installs as a single voice until the catalogue lists its speakers.
  - Write `legilo-voice.json`, then rename the directory into place so installs are atomic.
  - Installing Kokoro takes about 27 s, almost all of it bzip2 decompression on one core. The model manager runs installs on a worker thread with progress.
- **User interface:** a filterable list showing name, engine, accent, quality, size, licence and installed state.
  - Actions: download, remove, set as default, play sample.
  - The sample is synthesised locally once a voice is installed; before that, there is a link to the online sample.
- **Licences:** shown before download. Some Piper voices are licensed for non-commercial use only; these are flagged clearly.
- **Kokoro:** the English Kokoro model is a single download with several speakers. The UI lists each speaker as its own selectable voice, sharing one model on disk and in memory.

### 7.11 Reader window

#### 7.11.1 Layout

```
+---------------------------------------------------------------------------+
| [Open] [Paste] [Edit|Read]  [Preview]              Voice: [Alba v]  [Export]|
+---------------------------------------------------------------------------+
|                                                                           |
|  ...the previous sentence is shown normally. [The current sentence is     |
|  highlighted and the view follows it.] The next sentence...               |
|                                                                           |
+---------------------------------------------------------------------------+
| [|<] [<<] [ > / || ] [>>] [>|]   Sentence 42 / 310   Speed [-] 1.2x [+]   |
+---------------------------------------------------------------------------+
```

#### 7.11.2 Modes

- **Edit mode.** An editable `QPlainTextEdit` for pasting or typing.
  - Entering Read mode runs the pipeline on the text (Auto detection as for the clipboard) and shows the result.
- **Read mode.** A read-only `QPlainTextEdit` showing `Document::cleaned`.
  - `QPlainTextEdit` rather than `QTextEdit`: it is much faster for large documents (NFR-08) and supports extra selections for highlighting.

#### 7.11.3 Highlighting

- **Current segment:** shown with `setExtraSelections`, using the palette's highlight colour at reduced opacity, so it works in light and dark themes.
- **Follow:** the view scrolls to keep the current segment roughly one third from the top.
- **Follow pausing:** if the user scrolls manually, follow pauses until playback moves to a new paragraph or the user clicks "Follow" in the status bar.

#### 7.11.4 Navigation

- **Click:** a single click places the start marker at the clicked segment (shown as a subtle outline).
  - Play starts from the marker.
  - Double-click starts immediately from that segment.
- **Keyboard:** the text keeps its normal caret movement and selection keys (arrows, Ctrl+arrows, Home, End). Keyboard and screen-reader users can then review the text freely (NFR-07). Playback keys don't take those over:

  | Key | Action |
  |---|---|
  | Space | Play or pause |
  | Enter | Play from the sentence at the caret |
  | Alt+Left / Alt+Right | Previous / next sentence |
  | Alt+Up / Alt+Down | Previous / next paragraph |
  | Alt+Home / Alt+End | Start / end |
  | `+` / `-` | Speed up / down |
  | Esc | Stop |

- The toolbar mirrors these.
- **Speak shortcut while the reader is open:** speak selection still works globally. When the reader is visible, the captured text is shown in it; otherwise it plays in the background. This is a setting.

#### 7.11.5 Resume

- **Document ID:**
  - Files: SHA-256 of the file's content. Moving or renaming the file keeps its position; editing it starts afresh. Hashing is cheap at the 20 MB size limit (7.14).
  - Pasted and captured text: SHA-256 of `original`. It does not depend on clean-up settings, unlike `cleaned`.
- **Storage:** `PositionStore` saves `(id, offset, settings, timestamp)` to `positions.json` in the app data directory.
  - `offset` is a character offset into `cleaned`, not a segment index. Segment indices shift whenever segmentation rules or settings change.
  - `settings` is the serialised `CleanupSettings` that produced that `cleaned`.
  - Least-recently-used, at most 200 entries.
  - Saved on pause, stop and close, and every 30 s while playing.
- **Restoring:** if the document's current settings equal the stored ones, the offset applies directly, and the position is the segment containing it.
  - Otherwise, re-run the pipeline with the stored settings to rebuild the old `cleaned`. Map the offset into the new text with `diff_xIndex` (7.2.8), then take the segment containing it.
- **On open:** if a position exists and is not at the start, a non-modal bar offers "Resume from sentence N" or "Start from beginning".
- **Captures:** speak-selection captures are not stored by default, because short snippets aren't worth resuming. Setting: store captures longer than 2,000 characters.

### 7.12 Preview and clean-up settings

The preview is a toggleable pane in the reader. It also appears as the "Clean-up" page of the settings dialog, using the same widget.

```
+-------------------------------+-------------------------------+----------------------+
| Original                      | Spoken (what will be read)    | Clean-up settings    |
|                               |                               |                      |
| The re-                       | The results show...           | PDF reflow:          |
| sults show [12] that          |                               | (o) Auto ( ) Always  |
| ...                           |                               | ( ) Never            |
| 14                            |                               | Detected: PDF (0.71) |
|                               |                               |  [x] Page numbers    |
|                               |                               |  [x] Headers/footers |
|                               |                               |  [x] De-hyphenate    |
|                               |                               | URLs: [a link to v]  |
|                               |                               | Citations: [PDF v]   |
+-------------------------------+-------------------------------+----------------------+
| [Show differences: on]                       [Apply to this document] [Save as default]|
+--------------------------------------------------------------------------------------+
```

- **Spoken column:** `cleaned` with the speech rules (7.2.5) applied segment by segment. That is exactly what will be read, whereas the reader shows `cleaned` itself (7.2.8).
- **Diff:** computed with diff-match-patch between `original` and the spoken text, with semantic clean-up so the diff is readable rather than character-by-character noise.
  - Deletions are shown struck through in a muted red on the original side.
  - Insertions (for example, "a link to example.com") are shown in green on the spoken side.
  - A view with a single combined column can be toggled for narrow windows.
  - Large documents: set `Diff_Timeout` (about 0.5 s) so the diff degrades to a coarser result instead of blocking the update.
- **Live updates:** any setting change re-runs the pipeline and the diff on the text worker, debounced by 200 ms. The result is discarded if a newer run has started.
- **Detection explanation:** the detection score and the top contributing features are shown under the mode selector.
- **Scope:**
  - "Apply to this document" applies the settings to the current document only.
  - "Save as default" stores them in settings.
  - Changes during playback re-segment the document. The playing position is an offset into the old `cleaned`. Map it into the new `cleaned` with `diff_xIndex` over a diff of the two (7.2.8), and resume from the segment containing the result.

### 7.13 Audio export

- **Export dialog:**
  - Format: WAV, FLAC, Ogg Vorbis, Opus or MP3.
  - Quality (per format), voice, speed, file path, "also write SRT subtitles", and range (whole document, or from the current sentence).
- **Rendering:** the export worker runs its own synthesis loop, separate from playback, so playback can continue.
  - It loads its own `OfflineTts` instance of the voice with 1 thread. It never shares an instance with playback, and never competes for playback's threads.
  - The cost is one more model in memory while exporting, above the NFR-02 budget.
  - If concurrent synthesis turns out not to be safe (7.3.1), the loop takes the process-wide synthesis lock per segment and gives way to playback.
  - Speech rules apply per segment, as in playback.
- **Writing:** libsndfile writes progressively. Inter-segment gaps are inserted as silence, matching playback.
  - Opus in libsndfile accepts 8, 12, 16, 24 and 48 kHz. Kokoro (24 kHz) needs no resampling. Piper (22.05 kHz) is resampled to 24 kHz first, with miniaudio's resampler.
- **Subtitles:** one SRT cue per segment, with start and end times taken from cumulative frame counts.
- **Progress and cancel:** progress is shown in the dialog and the tray tooltip. Cancel deletes the partial file.
- **Metadata:** title (document title), artist ("Legilo - <voice name>"). Tags are written only where the format supports them.

### 7.14 Plain-text files and drag and drop

- **Encoding:**
  - Detect a UTF-8 or UTF-16 byte-order mark, then validate as UTF-8.
  - Otherwise fall back to a legacy codec.
    - Windows: the system's ANSI code page, through `QStringDecoder::System`. That is Windows-1250 on Central European systems, for example, so don't hard-code Windows-1252.
    - Linux: the locale is almost always UTF-8, which has already failed, so fall back to Windows-1252, the most common legacy encoding in practice.
  - Report a notice in the reader if a fallback was used, with a "Reopen with encoding" menu to choose another.
- **Drag and drop:** drop onto the reader window. Tray icons don't accept drops. Accepted: `text/plain` file URLs, and plain text dragged directly.
- **Open With:**
  - Linux: the `.desktop` file declares `MimeType=text/plain;` with `Exec=legilo --read %F`.
  - Windows: the installer adds an `OpenWithProgids` entry for `.txt`.
  - Legilo never registers itself as the default handler.
- **Size limit:** warn above 20 MB.
- **PDF detection:** files are not run through PDF detection by default. Plain-text files are usually deliberately formatted. Setting: use detection for files too.

### 7.15 PDF reading (stage 10)

#### 7.15.1 Library decision

| Option | For | Against |
|---|---|---|
| MuPDF | Best text extraction (structured text: blocks, lines, characters with fonts and positions); identical on both platforms; easy Windows build | AGPL-3.0 applies to the MuPDF part of the distributed combination (Legilo's own code stays GPL-3.0-or-later, section 11) |
| Poppler (poppler-qt6) | KDE standard (Okular uses it); in Debian; word bounding boxes; GPL keeps the project GPL | Harder Windows build (vcpkg or MSYS2); less structural information than MuPDF |

Recommendation: MuPDF. Decide at the start of stage 10. The licence no longer depends on the choice (section 11). Both sit behind the same interface, so the choice is replaceable:

```cpp
struct PdfLine { QString text; QRectF box; double fontSize; int page; };
struct PdfBlock { QList<PdfLine> lines; QRectF box; int page; };

class PdfExtractor {
public:
    virtual bool open(const QString &path, QString *error) = 0;
    virtual int pageCount() const = 0;
    virtual QList<PdfBlock> extractPage(int page) = 0;   // in reading order
};
```

#### 7.15.2 Layout processing

- **Headers and footers:** lines in the top or bottom 8% of the page whose digit-normalised text repeats on at least 40% of pages are removed. This uses positions, so it's more reliable than the clipboard heuristic.
- **Columns:** blocks are clustered by horizontal position. If two clusters with a clear gap are found, they are read left column then right column, per page.
- **Footnotes:** blocks in the bottom 20% of a page with a font size at least 15% smaller than the page's body text are footnotes. Setting: read in place, move to the end of the page, or skip (default: move to the end of the page).
- **Text clean-up:** the result goes through the reflow and speech rules, with PDF mode forced on.
- **Page navigation:** page boundaries are recorded as segment indices, for "start from page N" and a page indicator in the reader.
- **Scanned PDFs:** pages with no text layer produce a notice; OCR is future work.

#### 7.15.3 Entry points

- The reader's Open dialog, and drag and drop.
- Open With: the `.desktop` file adds `application/pdf`, and the Windows installer adds `OpenWithProgids` for `.pdf`.
- **Dolphin service menu**, installed to `/usr/share/kio/servicemenus/legilo.desktop` (Appendix C). It gives "Read aloud in Legilo" on PDF and text files.
- **Windows Explorer context menu.** A verb under `HKCU\Software\Classes\SystemFileAssociations\.pdf\shell\ReadInLegilo`.
  - On Windows 11 it appears under "Show more options".
  - Getting into the new top-level menu needs an `IExplorerCommand` handler plus package identity (MSIX or a sparse package). That is future work.

### 7.16 Logging and diagnostics

- **Logging:** `QLoggingCategory` per module (`legilo.text`, `legilo.tts`, `legilo.audio`, `legilo.platform`, ...).
  - Written to `legilo.log` in the app's local data directory, rotated at 1 MB with 3 files kept.
  - Debug categories can be enabled from the settings or with `QT_LOGGING_RULES`.
- **Help menu:**
  - "Copy diagnostics" collects the app version, Qt and KDE Frameworks versions, session type, sherpa-onnx version, loaded voices and the last 200 log lines.
  - Selected text is never logged: only lengths and detection scores.

---

## 8. Security and privacy

- No network access except user-started downloads and the optional catalogue refresh. Both are HTTPS. Downloads are verified with SHA-256, and a refreshed catalogue must be signed (7.10).
- Archive extraction blocks path traversal, absolute paths and symlinks (7.10).
- The IPC endpoint is accessible only to the current user (7.8).
  - Commands accept paths only to existing regular files.
  - Text cannot be injected through IPC in version 1, except via the `speak-text` command (`--speak-text`), which is refused unless `ipc/allowSpeakText` is on (7.8).
- Captured text is never written to disk, except:
  - resume positions, which store a hash and an index, not the text, and
  - documents the user explicitly saves or exports.
- No telemetry and no crash uploads.
- On Windows, the simulated-copy fallback is skipped for console windows, to avoid sending interrupts (7.1.3).
- Losing the audio device pauses playback instead of moving it to the speakers, so private text is not read aloud unexpectedly (7.4).

---

## 9. Testing strategy

### 9.1 Unit tests (Qt Test, run in CI on both platforms)

- **Text pipeline.** One test class per rule, with table-driven cases, including:
  - every issue in section 7.2.6, as a regression test
  - Unicode edge cases: ligatures, soft hyphens, superscripts, combining characters
  - speech rules applied per segment, and the spoken text matching the preview
- **Segmenter.** Abbreviations in each group (7.2.7), initials, decimals, ellipses, quotes, URLs and emails as single tokens, recursive maximum-length splitting, short-segment merging at paragraph edges, offsets matching the text.
- **Detector.** Feature calculations on small synthetic inputs.
- **Phonemiser.** Golden phoneme fixtures from piper-phonemize (7.3.1): punctuation and clause terminators, numbers, abbreviations, non-ASCII text, empty input.
- **Playback controller.** State transitions driven through fakes of the `TtsEngine` and `AudioOutput` interfaces (6.1). The fake engine returns tone bursts of known length; the fake output consumes frames on demand. This makes the following testable without real audio:
  - navigation, speed changes and pause
  - the short first chunk
  - the flush protocol and generation handling (7.4)
  - pausing on device loss
- **Audio queue.** The flush rules: stop and seek drop everything, and a speed change keeps the playing segment.
- **Model manager and installer.** Catalogue parsing and signature checks, checksum verification, safe extraction (with malicious test archives containing `../` and symlink entries), atomic install, and the registry ignoring temporary directories.
- **Position store.** Content-based IDs, offset storage, mapping an offset after settings change, and LRU eviction.
- **IPC protocol.** Request and reply parsing, refusal of `speak-text` when disabled, path validation, and the start-up sequence's handling of a stale socket.
- **Encoding detection** (7.14). Byte-order marks, valid and invalid UTF-8, and the legacy fallbacks.
- **Settings facade.** Defaults and types for every key in Appendix A.
- **Shortcut key mapping** (Windows): `QKeySequence` to virtual-key conversion, and the AltGr warning.

### 9.2 Regression corpus

- `tests/corpus/cleanup/<name>/input.txt` and `expected.txt` pairs. The test runs the pipeline in Auto mode and compares the output exactly.
  - **As built:** six pairs. The recipe and the terms and conditions match exactly. The other four cannot match from text alone, because a selection loses what showed their structure: a paragraph whose last line runs to the full width (the report, the novel), titles wrapped across lines in a large font (the paper, the newsletter), three columns (the newsletter), and, in the novel, a running header on its only page, which no rule can tell from a title. Their comparisons are expected failures with those reasons, and each pair must still recover at least as many paragraphs exactly as it did when the test was written (11 to 17 of 14 to 17), so a regression fails.
- `tests/corpus/detection/positive/` and `negative/`: at least 150 samples in total.
  - **Positive:** text copied from real PDFs via Okular, Firefox, Chromium and Adobe Reader, covering single and two-column layouts, and justified and ragged text.
  - **Negative:** emails, web articles, chat, code, poems, addresses, lists, Markdown, subtitles, and tables of numbers.
  - Split it into a tuning set (two thirds) and a held-out set (one third), fixed by a listing file in the repository. Tune weights and the threshold on the tuning set only. Tuning and scoring on the same samples overfits.
  - A test reports precision and recall on the held-out set. Targets:
    - precision at least 98%, because false positives damage text
    - recall at least 90%
- Start the corpus from the inputs you already tested the scripts with.
- **As built (`tests/corpus/`, stage 2):** 244 samples, 136 positive and 108 negative, plus private samples kept out of git.
  - Labels follow what the detector decides: positive means the line breaks are layout that reflow should undo. A menu, a table or a poem copied from a PDF is negative, because reflowing it would damage it.
  - A sample is labelled by what it holds, not by the document it came from. An excerpt of a paper can hold nothing but table cells or a chart's axis labels; `detection/relabelled.tsv` lists the 15 such excerpts, with the reason for each. An excerpt that mixes the two, such as a letter's address block above its wrapped body, stays positive.
  - Positives are excerpts, cut with `pdftotext` (poppler, as Okular uses), of six arXiv papers, a thesis and two PLOS ONE articles (CC BY 4.0); NIST SP 800-12, a Supreme Court opinion, a USGS fact sheet, IRS Publication 501, the Federal Rules of Evidence and two letters to Congress (works of the United States government); and ten documents typeset for the corpus with LibreOffice, whose clean text is known exactly and gives six clean-up pairs. 39 samples were selected by hand in Firefox, Okular and Chromium.
  - Negatives are written for the corpus, plus public-domain poems and book prose, and the excerpts that hold no prose.
  - The sets are fixed by `detection/split.txt`, which keeps each document's excerpts and hand copies together. The documents held out are listed in `detection/heldout-documents.txt`: seven added after the detector's features were chosen. The first held-out documents had been looked at while choosing them, so they moved to the tuning set. Samples not cut from a document are split by a hash of their name, about a third held out. The held-out set has 47 positives and 40 negatives.
  - `tests/corpus/local/`, which git ignores, holds samples of private documents, for local tuning only.
  - `tst_detectioncorpus` scores the held-out set against the targets. Until precision reaches its target, that check is marked as an expected failure (section 12, stage 2 results).
  - The tools that make it all, and the licences, are in `tests/corpus/README.md`.

### 9.3 Integration tests

- **TTS** (`tests/integration/tst_synthesis.cpp`, label `integration`): synthesise a fixed sentence with cori-medium, the default voice, and with Kokoro v0.19.
  - Assert a non-empty result, the expected sample rate, and a duration within plausible bounds. Synthesis is not deterministic (7.3.1), so duration checks average several calls.
  - Stop: playback output ends within 100 ms, and the worker is free within one chunk's synthesis time (7.3.1). Added with the synthesis worker.
  - Concurrent `Generate` calls, on one instance and on two (7.3.1).
  - The voices are pinned in `cmake/Dependencies.cmake` and fetched with `scripts/fetch-deps --test-voices` (about 390 MB), which `scripts/check` runs; CI caches them. A test whose voice is absent is skipped. The suite takes about 11 s.
- **Audio output** (`tests/integration/tst_audiooutput.cpp`): `MiniaudioOutput` on miniaudio's null backend, which consumes audio in real time without hardware, so it runs in the container and the package build. Playback, stop and resume, and reopening at another sample rate. Device loss is in the manual checklist.
- **IPC:** start an instance, send each command from a second process, check the replies.
- **Export:** export a short document to each format, then reopen the file with libsndfile and check its duration.

### 9.4 Manual test checklist per release

Linux (Plasma Wayland and Plasma X11) and Windows 11, each:

- **Selection** from Okular, Firefox, Chromium, Kate, Konsole, LibreOffice, and an Electron app (for example, VS Code).
  - On Windows, also from Notepad, Word, Edge, Windows Terminal (fallback skipped) and Adobe Reader.
- **Keyboard layouts** (Windows): with Legilo running, type AltGr characters on a layout that uses them (for example, AltGr+S, C and X on a Polish layout) and confirm no shortcut fires.
- **Shortcuts:**
  - Change in System Settings and confirm the app updates.
  - Change in the app and confirm System Settings updates.
  - Take a shortcut already used by another app and confirm the conflict is reported.
- **Audio:** unplug headphones while speaking and confirm playback pauses rather than moving to the speakers; suspend and resume the machine.
- **Media controls:** play, pause and step from the Plasma media applet, the Windows media flyout and a headset button.
- **Lifecycle:** autostart on and off; install, upgrade and uninstall the package.

### 9.5 Development practice

Test-driven development is the default for all of `core`: write the failing test first, then the code.

- The text pipeline and segmenter suit it especially well: write the corpus pairs and rule tests before the implementation.
- The playback state machine is test-driven through the fakes described above.
- `platform` and `app` are covered by the integration tests (9.3) and the manual checklist (9.4), plus unit tests for any logic that can be separated from the platform API (such as key mapping).

### 9.6 Benchmarks

Performance budgets are measured in a separate benchmark job, not asserted in the unit tests. Timing assertions on shared CI runners are flaky. The job records its results and does not block merges:

- 5 MB through the text pipeline in under 2 s
- first audio with a warm voice (NFR-01), using the test model
- preview update for a typical page within 300 ms (stage 7)

A regression of more than 20% against the previous recorded run is reported in the job's summary.

---

## 10. Build, CI and packaging

### 10.1 Build

- **CMake presets:** `linux-debug`, `linux-release`, `windows-debug`, `windows-release`. All use Ninja.
- **Compiler warnings:** warnings as errors in CI (`-Wall -Wextra -Wpedantic` on GCC; `/W4` on MSVC).
- **sherpa-onnx** (`cmake/SherpaOnnx.cmake`):
  - Built as an `ExternalProject` from a pinned release archive, with `BUILD_SHARED_LIBS=ON` and the Python bindings, tests, examples, binaries, websocket server and speaker diarisation off. It always builds as a release build.
  - Not `FetchContent`: sherpa-onnx's top-level `CMakeLists.txt` assumes it is the top level. It forces a release build, sets global output directories and definitions, and installs its headers, static libraries and pkg-config files, which would land in the package. Built apart, it installs into the build tree.
  - Legilo links its C++ API through the imported target `Legilo::SherpaOnnx`.
  - **Patched** before it is configured (`cmake/PatchSherpaOnnx.cmake`): for Piper voices, no pad token after the start marker, matching Piper's own runtime. Without the patch, some short sentences babble for seconds and sentence starts sound cramped (stage 1 results, 12). The script replaces exact text and fails the build if an upgrade has changed it. Report it upstream, so the patch can go.
  - **ONNX Runtime:** on Linux, Debian's `libonnxruntime` 1.21 (`libonnxruntime-dev`) rather than the prebuilt 1.28.2 that sherpa-onnx downloads by default. Configuration fails if it is missing, because sherpa-onnx would otherwise download its own without saying so. Windows uses a prebuilt release (stage 5). The comparison is in the stage 1 results (12).
- **Pinned archives, offline builds:** sherpa-onnx's configure step fetches nine more source archives, directly and through the projects it pulls in. `cmake/Dependencies.cmake` lists all ten with their SHA-256. `scripts/fetch-deps` downloads them into `third_party/downloads/` on the host, and the build copies them to where each fetch looks first, so the build itself never touches the network. `scripts/check` and `scripts/build-deb` run with the container's network switched off, so a missing archive fails there. Re-derive the list whenever sherpa-onnx is upgraded.
- **Private libraries:** on Linux, sherpa-onnx's two shared libraries (`libsherpa-onnx-c-api.so`, `libsherpa-onnx-cxx-api.so`) are installed to `/usr/lib/legilo/`, and the binary will find them through `INSTALL_RPATH` `$ORIGIN/../lib/legilo`. They never clash with system libraries.
- **Windows dependencies:** vcpkg manifest mode for libarchive and libsndfile (with MP3 support enabled), and later MuPDF. `windeployqt` collects the Qt runtime.

### 10.2 Continuous integration (GitHub Actions or GitLab CI)

| Job | Environment | Steps |
|---|---|---|
| `linux` | `debian:trixie` container | Install build dependencies from Debian; configure; build; run CTest; build the `.deb` with `dpkg-buildpackage -us -uc -b`; run `lintian`; upload the artefact |
| `windows` | Windows Server 2022 runner | Install Qt as decided in 4.2 (install-qt-action / aqtinstall); vcpkg (cached); configure; build; run CTest; `windeployqt`; build the Inno Setup installer; upload the artefact |
| `lint` | `debian:trixie` container | Run `clang-format --dry-run -Werror`; run `clang-tidy` with the repository configuration over `compile_commands.json` |
| `bench` | `debian:trixie` container | Run the benchmarks (9.6); record results; does not block merges |

Cache the sherpa-onnx build and the test model between runs.

### 10.3 Debian package

- A `debian/` directory with `debhelper-compat (= 13)`, `dh $@ --buildsystem=cmake+ninja`, and `Build-Depends` taken from section 5, without `devscripts` and with `help2man`. `debian/control` is the authoritative list: the build container installs from it.
- **Package name:** `legilo`, architecture `amd64`.
- **Versioning:** from the first release (stage 3), semantic versioning: `0.x.y` until stage 9, then `1.0.0`.
  - The Debian version is the upstream version plus `-1`.
  - `CHANGELOG.md` and `debian/changelog` are updated with every release.
- `Depends` are generated by `dh_shlibdeps`. It resolves the private libraries through their own `$ORIGIN` RUNPATH, so it needs no `-l`, and `dh_makeshlibs` generates no shlibs file for them. Their unversioned sonames draw a harmless "can't extract name and version" warning.
- **Installed files:**

  | File | Location |
  |---|---|
  | Binary | `/usr/bin/legilo` |
  | Manual page | `/usr/share/man/man1/legilo.1.gz`, generated from `--help` with `help2man` at build time |
  | Private libraries | `/usr/lib/legilo/` |
  | Desktop file | `/usr/share/applications/io.github.example.legilo.desktop` |
  | Icons | `/usr/share/icons/hicolor/.../apps/io.github.example.legilo.*` |
  | AppStream metadata | `/usr/share/metainfo/io.github.example.legilo.metainfo.xml` |
  | Service menu (stage 10) | `/usr/share/kio/servicemenus/legilo.desktop` |
  | Licence notices | `/usr/share/doc/legilo/copyright` (DEP-5), including third-party notices |

- **Recommends:** none at first. If the `wl-paste` contingency is used (7.1.2), add `wl-clipboard` to `Depends`.
- **Distribution:** stage 3 ships `.deb` files on the project's releases page. Stage 9 adds a signed apt repository (`aptly` or `reprepro`, hosted on GitHub Pages or similar) so users get updates through `apt`.
- **Official Debian archive:** out of scope. Debian would want sherpa-onnx and the libraries compiled into it packaged separately rather than built from downloaded archives.

### 10.4 Windows installer

- **Inno Setup script** in `packaging/windows/`.
  - Per-user install by default (no administrator prompt) into `%LOCALAPPDATA%\Programs\Legilo`.
  - Start Menu shortcut; optional autostart; Open With registration for `.txt` (and `.pdf` from stage 10); uninstaller removes registry entries.
  - Voices and settings are kept on uninstall unless the user ticks "remove my voices and settings".
- **Code signing:** unsigned installers trigger SmartScreen warnings. Options to evaluate at stage 9:
  - SignPath Foundation (free for open-source projects that qualify)
  - Microsoft's Trusted Signing service
  - an OV certificate
- **winget:** a winget manifest once releases are stable (stage 9).

---

## 11. Licensing

- **Legilo's own code:** GPL-3.0-or-later. This is required, because Legilo links espeak-ng (GPL-3.0-or-later) for phonemisation (7.3.1).
- **If MuPDF is chosen at stage 10:** no relicensing is needed. Section 13 of the GPLv3 explicitly allows a GPLv3 work to be combined with AGPLv3 code. Legilo's own code stays GPL-3.0-or-later, and the MuPDF part of the combination keeps its AGPL terms. The AGPL's network clause has no practical effect on an offline desktop app.
- **Third-party notices:** a `THIRD-PARTY-NOTICES` file is generated from a list maintained in the repository, and shown in the About dialog. Until then, `debian/copyright` records the code compiled into the private libraries: sherpa-onnx and its dependencies, under Apache-2.0, MPL-2.0, Expat, BSD-2/3-Clause, Zlib and the Unicode data licence, all compatible with GPL-3.0-or-later. It also carries SCOWL's notice, which its licence requires with every copy, for the word lists compiled into Legilo.
- **Voice licences:** shown in the model manager (7.10) and written into each voice's directory. Non-commercial voices are flagged.
  - Voices trained or fine-tuned on the lessac dataset are flagged in the same way. Its licence (Blizzard Challenge 2013) is for research use only. This covers lessac itself and voices fine-tuned from it, such as alba and joe. Each voice's model card says how it was trained.

---

## 12. Delivery stages

Sizes are relative: S (small), M (medium), L (large), XL (extra large). As a rough guide for one developer working part-time, S is about one to two weeks, M two to four, L four to eight, and XL longer. Treat these as rough; the Windows and Wayland stages carry the most uncertainty.

Each stage ends in something usable or testable and has explicit exit criteria. Keep using the existing scripts until stage 3 replaces them.

### Stage 0 - Foundations (S)

- **Goal:** a repository where everything builds and tests on both platforms from day one, so Linux-only assumptions are caught early.
- **Scope:**
  - Repository layout (6.5), CMake presets, warnings, `clang-format`.
  - Empty `core`, `platform` and `app` targets.
  - Qt Test wired into CTest.
  - Logging categories; the `Settings` facade skeleton.
  - A trixie container for builds; CI for Linux and Windows (10.2), including a minimal `debian/` directory that produces an installable `.deb` of a "hello" binary.
  - `README.md` with build, test and run instructions, kept current from here on.
  - The Windows Qt version decision (4.2).
  - **Spikes** on the cheap-to-test unknowns that could change the plan, each about a day:
    - Read the selection with `KSystemClipboard` on trixie Plasma Wayland, from Okular, Firefox and Kate, without window focus (7.1.2).
    - List the pre-converted Piper voices sherpa-onnx publishes and their licences, to settle the first-run default (open question 3).
    - Measure first-audio latency for a short and a 300-character sentence with sherpa-onnx's own command-line tool, Piper medium and Kokoro (NFR-01).
- **Exit criteria:**
  - CI is green on both platforms.
  - The `.deb` installs and runs on a clean trixie machine.
  - Spike results are recorded in this document, with the plan adjusted where they require it.

#### Stage 0 results (28 October 2026)

**Latency.** Measured with sherpa-onnx's own command-line tool on an Intel Core i7-10610U (4 cores, 8 threads, a 2020 laptop CPU). The figures are synthesis time only, with the voice already loaded, as the mean of three runs. They were taken with v1.13.8, because 2.x needs the phonemiser that doesn't exist yet (see below). Synthesis runs the same ONNX models in both versions.

| Voice | Threads | 44 characters (2.5 s of audio) | 317 characters (17 to 18 s of audio) | Real-time factor, long text |
|---|---|---|---|---|
| Piper cori medium | 1 / 2 / 4 | 0.27 / 0.20 / 0.16 s | 2.1 / 1.9 / 1.8 s | 0.12 / 0.11 / 0.10 |
| Piper cori high | 1 / 2 / 4 | 2.6 / 1.6 / 1.3 s | 19.2 / 13.6 / 14.5 s | 1.12 / 0.80 / 0.84 |
| Kokoro v0.19 fp32 | 1 / 2 / 4 | 4.5 / 3.0 / 2.7 s | 24.2 / 17.6 / 18.0 s | 1.33 / 0.97 / 0.99 |
| Kokoro v0.19 int8 | 1 / 2 / 4 | 7.0 / 5.0 / 4.4 s | 38.6 / 24.7 / 23.2 s | 2.13 / 1.36 / 1.28 |

- Piper medium meets NFR-01 comfortably. An 80-character first chunk takes about 0.3 s.
- Piper high and Kokoro run at about real time at best on this CPU, so they miss NFR-01 and can't stay ahead of playback. The plan now treats them as voices for fast CPUs (7.3.2).
- Kokoro int8 is slower than fp32 here: this CPU has no VNNI instructions.
- Above 2 threads there is little gain, which supports the default of half the cores, clamped to 1 to 4 (7.3.1).
- Loading a voice took a further 1.4 to 2.7 s, including process start. Hence the background preloading (7.3.2).
- Memory was not measured. It is checked in stage 1 against NFR-02.

**sherpa-onnx 2.0 removed phonemisation.** Version 2.0.0 (23 October 2026) dropped espeak-ng and piper-phonemize, to keep sherpa-onnx free of GPL code. Piper and Kokoro voices now need phonemes supplied by the caller.

- Decision: track 2.x and give Legilo its own `Phonemiser` on espeak-ng (7.3.1), rather than pinning v1.13.8, the last release with espeak-ng built in. v1 gets no further fixes, and moving off it later would need the same work in the middle of the project.
- Debian trixie's `libespeak-ng` 1.52 exports `espeak_TextToPhonemesWithTerminator`, the function piper-phonemize is built on, so the Linux build can use the distribution package.
- Effects: stage 1 grows by about a week. Section 5 has a new dependency, and the licensing reason in section 11 changes.

**Voices and licences (open question 3).** sherpa-onnx publishes about 45 pre-converted English Piper voices plus Kokoro. Their model cards show three kinds of licence:

- **Public domain, with a clean lineage:**
  - cori (en_GB) and kristin (en_US) were trained from scratch on LibriVox recordings.
  - john (en_US) was fine-tuned from kristin.
- **Research only:** the lessac dataset's licence (Blizzard Challenge 2013). This covers lessac and voices fine-tuned from it, including alba and joe. Flagged as described in section 11.
- **Non-commercial (CC BY-NC):** hfc_female, hfc_male, semaine and l2arctic.
- Decision: the first-run default is `en_GB-cori-medium`. Kokoro's weights are Apache-2.0.

**Windows Qt (open question 7).** Qt 6.12, as described in 4.2.

**Selection capture on Wayland.** A test program with no window polled the primary selection and the clipboard through `KSystemClipboard` every 500 ms on Plasma 6.3.6 Wayland. It never had focus.

- Selections highlighted (not copied) in Firefox, Kate and a PDF in Okular all arrived, up to 5,537 characters. Each read took at most 3 ms. Highlighting left the clipboard untouched, as it should.
- When focus moved to Konsole, the primary selection reverted to Konsole's own earlier selection. Later, an application cleared it.
  - The primary selection is whatever the system last set it to, not necessarily what is highlighted on screen.
  - A global shortcut doesn't move focus, so speak-selection reads the selection the user just made.
  - An empty selection takes the existing path: notify, or fall back to the clipboard (7.1).
- Conclusion: `KSystemClipboard` is suitable. The `wl-paste` contingency (7.1.2) stays documented but is not expected to be needed.

### Stage 1 - Speech core and command line (M)

- **Goal:** text in, audio out, from the command line.
- **Scope:**
  - sherpa-onnx 2.x, pinned, built as an `ExternalProject` against Debian's ONNX Runtime (10.1).
  - The `Phonemiser` on espeak-ng, test-first against golden fixtures from piper-phonemize (7.3.1). This adds about a week to the stage.
  - The `TtsEngine` interface and its sherpa-onnx implementation, `Voice` and `VoiceRegistry`.
  - `ModelInstaller` and `legilo --install-voice ARCHIVE` (FR-28). Download sherpa-onnx's `vits-piper-en_GB-cori-medium` archive by hand for now; the command installs it.
  - The `AudioOutput` interface and its miniaudio implementation; `AudioQueue` with the flush protocol (7.4); the synthesis worker with look-ahead and the short first chunk (7.5).
  - A minimal `PlaybackController` (play, pause, resume, stop, speed).
  - A temporary command-line front end: `legilo --speak "text"` and `--speak-file`. Stage 3 replaces them with `--speak-text` and `--read` (Appendix B).
  - Integration test for TTS (9.3), including the thread-safety check (7.3.1) and a check that Kokoro takes the Piper phoneme stream.
- **Exit criteria:**
  - Speaks a long text file smoothly, with no gaps between sentences.
  - First audio arrives within the NFR-01 budget with a warm voice.
  - Pause and stop silence playback within 100 ms.
  - Speed changes are heard within one sentence.
  - Windows moved to stage 5 (decided 30 October 2026, see the stage 1 results).

#### Stage 1 results (29 October 2026)

**Phonemiser.** Done, test-first against the golden fixtures (7.3.1). With piper-phonemize's own espeak-ng data, its output matches all 42 fixture cases. With Debian's data it matches every British English case; American English differs in one vowel, which the test records.

**ONNX Runtime (Linux).** sherpa-onnx 2.0.1 was built twice in the trixie container: against Debian's `libonnxruntime` 1.21, and against the prebuilt 1.28.2 it downloads by default. Both synthesised cori-medium and Kokoro v0.19 fp32 from the `Phonemiser`'s output. Synthesis time for the 317-character text, best of three, on the same i7-10610U as stage 0, as real-time factors:

| Voice | Threads | Debian 1.21 | Prebuilt 1.28.2 |
|---|---|---|---|
| Piper cori medium | 1 / 2 / 4 | 0.15 / 0.12 / 0.09 | 0.14 / 0.11 / 0.09 |
| Kokoro v0.19 fp32 | 1 / 2 / 4 | 1.27 / 0.94 / 0.82 | 1.25 / 0.92 / 0.82 |

- The speed is the same. sherpa-onnx's code needs nothing newer than ONNX Runtime API 17.
- Decision: link Debian's. It saves a 26 MB library in the package, gets Debian's security updates, and avoids shipping a binary from a personal GitHub mirror (`csukuangfj/onnxruntime-libs`) rather than from Microsoft. It is often installed already: `gstreamer1.0-plugins-bad` depends on it.
- Cost: each time a voice loads, Debian's build writes 567 "Schema error: Trying to register schema ... already registered" lines to stderr. Synthesis is unaffected, and the prebuilt one writes nothing. The cause is probably its separate system `libonnx` registering the same operator schemas, but that is unconfirmed, and no bug report was found. Report it to Debian; Legilo does not hide stderr, which would hide real errors too (13).
- Cost: sherpa-onnx is developed against 1.28.2, seven minor versions ahead. If a later release needs a newer API, the fallback is bundling (13).

**Build.** sherpa-onnx is built as an `ExternalProject`, not with `FetchContent`, and all of its downloads are pinned and fetched beforehand (10.1). The first build takes about 4 minutes and adds about 4.2 MB of private libraries to the package.

**TTS engine.** `TtsEngine`, `SherpaTtsEngine`, `Voice` and `VoiceRegistry` are done, with the integration suite (9.3). It settled:

- Kokoro accepts the Piper phoneme stream.
- Concurrent synthesis on one engine and on two gave sound speech every time, so there is no process-wide lock (7.3.1).
- Synthesis is not deterministic, for Piper or Kokoro, and speed does not scale duration in proportion: cori at 2.0 takes about 0.66 of the time (7.3.1).
- Kokoro's metadata names `en-us` for all eleven speakers, including the four British ones, so a speaker can override the espeak-ng voice in its manifest (7.3.2).

**Audio.** `AudioQueue`, `AudioOutput` and `MiniaudioOutput` are done. The queue's flush protocol changed to deliver every flush in order, after a race in the original design came to light (7.4); a test pins it, and ThreadSanitizer found no races with the producer, controller and consumer at work together. miniaudio comes from Debian's package rather than a vendored copy (5).

**Listening checks (30 October 2026).** A throwaway program played the speech path through a real device (Plasma 6.3.6, PipeWire, wired headphones):

- cori plays through miniaudio's PulseAudio backend (PipeWire's emulation) with 75 ms of buffering. The first sentence was queued 60 to 260 ms after start, with the voice already loaded.
- cori's first few words of a text can sound rushed, with odd pitch changes. Piper's own engine (`piper-tts`) does the same with the same model, so it is the voice, not Legilo.
- Some short sentences made cori babble for seconds ("Eight.", "Open."), and the first words of a text sounded cramped. Traced the same night: sherpa-onnx puts a pad token after the start marker that Piper's own runtime does not, and with it "Eight.", "Open." and "Eat." ran past 1 s in 18 to 20 calls of 20, against 0 to 5 for piper-tts. Running the model directly with only that pad changed reproduced it, and the cramped start was heard to go. Legilo now patches sherpa-onnx (10.1), after which it matches or beats piper-tts on every word measured. cori still babbles on some isolated words in both runtimes ("Yes."), so segmentation avoids one-word segments (7.2.7).
- Pause and resume are clean: the callback pulls nothing while stopped, and `stop()` returns at once.
- Kokoro's British speaker `bf_emma` sounds better with British phonemes (`en`) than with the `en-us` in the model's metadata, so the catalogue sets `espeakVoice` for Kokoro's British speakers (7.3.2).
- Unplugging wired headphones stops playback, which does not move to the speakers: PipeWire reports the change as a reroute, which miniaudio passes on (7.4).

**Playback.** `SynthesisWorker`, `PlaybackController`, the short first chunk (7.5) and `legilo --speak` / `--speak-file` are done. The controller is tested through fakes of `TtsEngine` and `AudioOutput` (9.1). Two simplifications, to revisit when later stages need them:

- There is no Stopping state: `stop()` stops the device at once and returns to Idle, and the flush discards the queue when the device next runs.
- Text is split by a stand-in for the Segmenter (`app/SimpleSegmenter`): paragraphs, sentence ends and no one-word segments, without abbreviations. Stage 2's Segmenter replaces it, with the stage 1 commands.

Against the exit criteria, on 30 October 2026:

| Criterion | Result |
|---|---|
| Speaks a long text file smoothly | Met for a four-paragraph file: the queue never ran dry, and the listener heard no gaps. cori's prosody, with uneven pitch between words, is the voice's own (stage 4 chooses voices). |
| First audio within NFR-01 with a warm voice | Met: the first chunk was queued 60 to 260 ms after start, within the 500 ms budget. |
| Pause and stop silence playback within 100 ms | Met by measurement: the device stops at once, with 75 ms of buffering downstream. |
| Speed changes heard within one sentence | Met in the controller's tests; the command line cannot change speed while speaking, so not yet heard. |
| Works on both platforms | Moved to stage 5. The Windows build has not been set up: espeak-ng, ONNX Runtime and miniaudio need Windows sources (5, 10.1), and there is no Windows machine or CI to build on yet. Stage 1 closes on Linux. |

**Installer.** `ModelInstaller` and `legilo --install-voice` are done, with stricter extraction than planned and tests on malicious archives built as the tests run (7.10). cori-medium installs in about a second, Kokoro in about 27 s.

### Stage 2 - Text clean-up and PDF detection (M)

- **Goal:** the full text pipeline, test-first.
- **Scope:**
  - Normaliser (7.2.2), detector (7.2.3), reflow (7.2.4), speech rules (7.2.5), segmenter (7.2.7), `CleanupSettings`, and the `Document` model.
  - The bundled word list.
  - The regression corpus (9.2), starting from your existing samples.
  - All fixes in 7.2.6, each with a regression test.
  - Command-line options: `--clean-only` (print the spoken text: `cleaned` with speech rules applied) and `--explain` (print the detection features).
- **Exit criteria:**
  - Corpus tests pass.
  - Detection reaches at least 98% precision and 90% recall on the held-out set (9.2).
  - Output is at least as good as `speak-tts --clean-only` on all of your samples.
  - The 5 MB performance budget is met in the benchmark job (9.6).

#### Stage 2 results (6 November 2026, in progress)

Done so far: the normaliser, segmenter, word list, reflow and whitespace steps, the regression corpus (9.2), PDF detection (7.2.3), `CleanupSettings`, the `Document` model and `CleanupPipeline` (7.2.1, 7.2.8), and the clean-up pair test (9.2). Still to do: the speech rules (7.2.5), `--clean-only` and `--explain`, the comparison with `speak-tts --clean-only`, and the 5 MB benchmark.

**PDF detection meets its recall target but not its precision target.**

| Detector | Scored on | Precision | Recall |
|---|---|---|---|
| First draft's features and weights | first held-out set | 0.82 | 0.43 |
| Fitted features (7.2.3) | first held-out set | 1.00 | 0.86 |
| Fitted features, refitted on the larger tuning set | fresh held-out set | 0.935 | 0.915 |
| `wrap` needs three words on the line, refitted | fresh held-out set | **0.915** | **0.915** |
| Target (9.2) | | 0.98 | 0.90 |

- The first held-out set came from about five documents, so one document decided a fifth of its recall, and its misses (a letter's address block above short paragraphs) were looked at while choosing the `wrap` feature. It moved to the tuning set, and seven documents in layouts the corpus lacked were added as a fresh held-out set, labelled by reading them before the detector ran on them.
- On the fresh set every negative written for the corpus passes. The three false positives are non-prose inside the new PDFs: equation fragments from the thesis and two IRS tables. Their short lowercase fragments look like wrapped lines to `wrap`. One of the tables, whose cells wrap, was a borderline label.
- Requiring three words before a line can wrap was chosen on the tuning set, where it raised recall from 0.90 to 0.955 (and the private samples' from 0.74 to 0.82) at the same precision. On the held-out set it changed little: the thesis's equation fragments hold enough letter runs to pass, and the larger `wrap` weight took a fourth IRS table over the threshold. It is kept, since dropping it on that score would be choosing by the held-out set.
- Adding the tuning PDFs' own tables, equations and charts as negatives (23 excerpts cut where short lines are densest) was tried and dropped on the tuning set's evidence: none had been a false positive, and the refit lost tuning recall. Their tables are numbers and short labels, while the held-out failures are tables of wrapped sentences and letter-heavy equations, which the tuning documents do not have. Finding documents that do would mean choosing them by the held-out failures.
- The precision check is marked as an expected failure in `tst_detectioncorpus`, not lowered: Qt fails the test with XPASS once the target is met. Detection is left there for stage 2. The next data, both to tune on and to score, comes from selections saved during real use at stage 3, which will hold tables and equations as readers really select them, not chosen to match these failures.

**More about how the viewers copy**, from the second round of selections:

- Firefox made a table hard to select, and the text below a table could not be selected until the next page.
- Okular highlighted three-column text under a table as if it were one line.
- Chromium copied a linked phrase one letter per line ("Q", "u", "a", ...). Reflow would join those letters with spaces and speech would spell the word out. Joining runs of single-letter lines without spaces is a candidate reflow rule.
- A copy helper left running saves whatever else is selected on the desktop, because the selection is shared. The corpus's helper now saves a selection only when most of it comes from a corpus PDF, and stops after a set number of saves.

**Real selections differ from `pdftotext`, and from each other.** Text selected by hand in Firefox (pdf.js), Okular (poppler) and Chromium (PDFium), compared with `pdftotext`'s excerpts of the same kind of text (aggregate figures only; private samples included):

| Source | Mean line length | Blank lines | Of those, mid-sentence | Lines ending in "-" | Lines under 20 characters |
|---|---|---|---|---|---|
| `pdftotext` | 63 | 7.9% | 79% | 5.5% | 20% |
| Okular | 57 | 0% | - | 8.9% | 24% |
| Firefox | 46 | 38.6% | 81% | 6.5% | 40% |
| Chromium | 74 | 0% | - | 0.2% | 11% |

- **Okular** keeps no blank lines: paragraph breaks are lost, so reflow would join a whole selection into one paragraph, broken only by its heading rule.
- **Firefox** puts a blank line between many lines, and they are not paragraph breaks: the line before ends mid-sentence in 877 of 1080 cases, and half of its hyphenated breaks have a blank line between the halves.
- **Chromium** keeps no blank lines either, and almost no line ends in a hyphen: a hyphenated break seems to arrive joined into one line, with a space after the hyphen ("inter- ferometric").
- `pdftotext`'s blank lines are mostly page and column breaks too (79% fall mid-sentence). In no source does a blank line reliably mark a paragraph.
- Reflow (7.2.4) and detection (7.2.3) both read a blank line as a paragraph break. They have to change, test-first against these samples, when the detector is built:
  - In PDF mode, a blank line inside a sentence (the line before ends without terminal punctuation and the next continues it) is not a paragraph break, and de-hyphenation works across it. Done (7.2.4, rule 3).
  - In PDF mode, "word- word" inside a line (Chromium's form of a hyphenated break) joins when the word list confirms the joined word. Done (7.2.4, rule 3).
  - Where a copy has lost its paragraph breaks, a paragraph ends after a line that ends a sentence and is clearly shorter than the text's usual line length. Done (7.2.4, rule 5), with headings that have no full stop and clause numbers. Building it showed that de-hyphenation merged two lines into one, which made every other line look short; that is fixed too (rule 3).
  - Detection's features must not depend on blank lines marking paragraphs. Done: detection ignores blank lines (7.2.3).
- `pdftotext` itself sometimes merges two lines into one, leaves a space inside a hyphenated word ("ac count"), and runs a word hyphenated at the foot of a page into the footer ("somePage 1 of 5"). The corpus keeps these as found.
- Keyboard selections may not reach the primary selection (7.1.2).

### Stage 3 - Linux tray app: speak selection and clipboard (M)

- **Goal:** the first real release; replaces the scripts on your machine.
- **Scope:**
  - Before anything ships: the final name and application ID (open question 1), and the versioning scheme, `CHANGELOG.md` and `debian/changelog` (10.3).
  - `KdeTextSource`, plus `WlPasteTextSource` if the stage 0 spike showed it is needed (7.1.2). `KGlobalAccelShortcuts` with two-way sync, the shortcut actions (7.6.2).
  - Tray (7.7), notifications, autostart.
  - Single instance with the lock-file start-up, IPC and the full command line (7.8).
  - The full playback state machine (7.5), including the speak-while-speaking behaviour.
  - Settings dialog pages: General, Voice (choose from installed voices, speed), Shortcuts, and a basic Clean-up page (mode and rule toggles, no preview yet).
  - The `.desktop` file, icons and AppStream metadata.
  - The `.deb` published on the releases page.
- **Exit criteria:**
  - On Plasma Wayland, the default shortcut reads the selection from Okular, Firefox and Kate.
  - The clipboard shortcut works.
  - Shortcuts are editable in System Settings and in the app, and stay in sync.
  - Survives logout and login with autostart.
  - Section 9.4 checklist passes for Linux.

### Stage 4 - Model manager and Kokoro (M)

- **Goal:** install voices from the UI; add Kokoro.
- **Scope:**
  - Catalogue with signed refresh, downloader, verification and removal (7.10). Safe extraction and atomic install reuse `ModelInstaller` from stage 1.
  - Model manager dialog with licence display and samples.
  - Kokoro engine support, with speakers listed as voices.
  - The least-recently-used voice cache.
  - First-run flow: if no voice is installed, offer the recommended default (Piper `en_GB-cori-medium`, about 67 MB).
- **Exit criteria:**
  - A clean install reaches a working voice in three clicks.
  - Checksum failure and malicious-archive tests pass.
  - Kokoro meets NFR-01 and NFR-02 on your hardware, or the measured limits are recorded.

### Stage 5 - Windows port (L)

- **Goal:** feature parity with stage 4 on Windows.
- **Scope:**
  - The Windows build itself, moved here from stage 1: espeak-ng built from source with its data bundled, a prebuilt ONNX Runtime for sherpa-onnx, miniaudio and the other archives pinned in `cmake/Dependencies.cmake`, vcpkg for libarchive (10.1). The stage 1 exit criteria (speech core and command line) then hold on Windows too.
  - `Win32Shortcuts` (7.6.4), with the Windows defaults checked against reserved combinations (7.6.2).
  - `UiaTextSource` with `IUIAutomation2` timeouts, the simulated-copy fallback, the excluded-apps list and console detection (7.1.3).
  - Clipboard owner hint.
  - `RunKeyAutostart`; tray and notifications.
  - Named-pipe IPC with the session ID in the name (7.8); Qt handles the pipe itself.
  - Inno Setup installer (10.4) with Open With for `.txt`.
  - Windows defaults for shortcuts.
- **Exit criteria:**
  - The stage 1 speech core passes its tests and reads a long file smoothly on Windows 11.
  - Section 9.4 checklist passes on Windows 11.
  - Selection reading works in Notepad, Word, Edge, Firefox and Adobe Reader.
  - The fallback is correctly skipped in Windows Terminal.
  - Typing AltGr characters never triggers a shortcut (9.4).
  - Installs and uninstalls cleanly without administrator rights.

### Stage 6 - Reader window (L)

- **Goal:** a full reader for longer text.
- **Scope:**
  - Reader window (7.11): Edit and Read modes, highlighting with follow, click and double-click to start, keyboard and toolbar navigation, sentence and paragraph stepping, speed control.
  - The resume bar and `PositionStore`, with content-based IDs and offsets (7.11.5).
  - Plain-text files: open dialog, drag and drop, encoding detection with "Reopen with encoding", Open With, `--read`.
  - Tray integration (left click toggles the reader).
  - Speak-selection text optionally shown in the reader.
  - Media controls (FR-29, 7.7): `MprisControls` and `SmtcControls`.
- **Exit criteria:**
  - Smooth highlighting in a 5 MB file.
  - Navigation commands are covered by controller tests.
  - Resume works across restarts, and after a change of clean-up settings.
  - Media controls work from the Plasma media applet and the Windows media flyout.
  - Keyboard-only use is possible.
  - Tested with Orca on Linux and Narrator on Windows.

### Stage 7 - Preview with clean-up settings (M)

- **Goal:** see and control what the clean-up does.
- **Scope:**
  - Preview pane (7.12) with diff highlighting, the clean-up settings panel, the detection explanation, live updates, apply to this document or save as default.
  - The same panel on the settings dialog's Clean-up page.
  - Re-segmenting during playback, keeping the position.
- **Exit criteria:**
  - Toggling any rule updates the preview within about 300 ms for a typical page of text.
  - The diff is readable rather than noisy on the corpus samples.

### Stage 8 - Audio export (S)

- **Goal:** turn a document into an audio file.
- **Scope:**
  - Export dialog and worker (7.13); libsndfile formats; the Opus resampling path; SRT output; progress and cancel.
- **Exit criteria:**
  - Each format exports, reopens and has the expected duration.
  - Subtitle timings stay within 100 ms of the audio.
  - Playback continues during export.

### Stage 9 - Release engineering (M)

- **Goal:** something other people can install and keep updated.
- **Scope:**
  - Signed apt repository.
  - Windows code signing (evaluate the options) and a winget manifest.
  - User documentation: a short user guide and a troubleshooting page (Wayland notes, Windows selection caveats). The README has existed since stage 0.
  - Translation infrastructure (`.ts` files, an `lupdate` target).
  - About dialog with licences.
  - "Copy diagnostics".
  - The `1.0.0` release (10.3).
- **Exit criteria:**
  - A fresh trixie machine can add the repository, install, and receive an update.
  - The Windows installer runs without an "unknown publisher" warning, if signing is obtained.

### Stage 10 - PDF reading (L)

- **Goal:** read PDF files directly.
- **Scope:**
  - Library decision and licence decision (7.15.1, 11).
  - `PdfExtractor`, layout processing (7.15.2), page navigation and "start from page".
  - Entry points (7.15.3): open, drag and drop, Open With, Dolphin service menu, classic Explorer verb.
  - Scanned-page notice.
- **Exit criteria:**
  - A corpus of at least 20 real PDFs (papers, reports, books, two-column journals) reads in the correct order, without headers, footers or page numbers.
  - "Read aloud in Legilo" works from Dolphin and Explorer.

### Stage summary

| Stage | Name | Size | Platforms | Main outcome |
|---|---|---|---|---|
| 0 | Foundations | S | Both | CI, packaging skeleton, spike results |
| 1 | Speech core and command line | M | Both | Text to audio |
| 2 | Clean-up and PDF detection | M | Both | Text pipeline with tests |
| 3 | Linux tray app | M | Linux | First usable release |
| 4 | Model manager and Kokoro | M | Both | Voices from the UI |
| 5 | Windows port | L | Windows | Parity on Windows |
| 6 | Reader window | L | Both | Highlighted reading |
| 7 | Preview and clean-up settings | M | Both | Visible, tunable clean-up |
| 8 | Audio export | S | Both | Audio files |
| 9 | Release engineering | M | Both | Installable, updatable |
| 10 | PDF reading | L | Both | PDF files |

Stages 4 and 5 can swap order. Doing Windows earlier exposes portability problems sooner, at the cost of a longer wait for the model manager on Linux.

---

## 13. Risks and mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| `KSystemClipboard` in Frameworks 6.13 misbehaves on trixie Wayland | Low | High | Stage 0 spike: selections from Firefox, Kate and Okular read without focus (12); `wl-paste` contingency selected at run time (7.1.2) |
| Pre-converted Piper voices missing or unsuitably licensed | Low | High | Checked in stage 0: public-domain voices exist; default `en_GB-cori-medium` (12) |
| First audio misses NFR-01 with long segments | Low | Medium | Short first chunk (7.5); Piper medium measured at about 0.3 s for the first chunk in stage 0 (12); benchmark job (9.6) |
| Default shortcuts clash with keyboard layouts or system combinations | Medium | Medium | No Ctrl+Alt defaults on Windows; checks in stage 5 and the manual checklist (7.6.2, 9.4) |
| Windows selection capture unreliable in some apps | High | Medium | UI Automation first, then simulated copy; clear notification; "speak clipboard" always works |
| Simulated Ctrl+C has side effects (terminals, apps that bind Ctrl+C differently) | Medium | Medium | Console detection; per-application exclusion list in settings |
| sherpa-onnx API changes or removes features between releases (2.0 removed phonemisation) | Medium | High | Pin the version; wrap it entirely in `TtsEngine`; upgrade deliberately; Legilo owns phonemisation (7.3.1) |
| Legilo's patch to sherpa-onnx (10.1) stops applying, or is dropped and the babbling returns, on an upgrade | Medium | Medium | The patch script fails the build if its target text changes; the integration test fails if the babbling returns (9.3); report it upstream |
| A later sherpa-onnx needs a newer ONNX Runtime than Debian's 1.21 | Medium | Medium | Pinned versions; the TTS integration test catches it; fall back to bundling the prebuilt runtime as private libraries (10.1) |
| Debian's ONNX Runtime floods stderr (567 lines per voice load), filling the journal | High | Low | Report it to Debian; voices load rarely, at start and on a voice change, because idle unloading is off by default (7.3.2) |
| Legilo's phonemes differ from piper-phonemize's, degrading voice quality | Low | Medium | Golden fixtures from piper-phonemize (7.3.1, 9.1); British English matches exactly, American differs in one vowel because of espeak-ng's data; listening check in the manual checklist |
| Kokoro and Piper high too slow on typical laptop CPUs | High | Low | Measured in stage 0 at about real time (12); marked `needsFastCpu`, with an underrun notification (7.3.2); Piper medium remains the default; GPU later |
| PDF detection false positives damage normal text | Medium | Medium | Conservative threshold; negative signals; corpus with false positives weighted double; Auto/Always/Never and preview |
| Voice licence restrictions (non-commercial voices) | Low | Medium | Licences shown and flagged; default voice chosen with a permissive licence |
| Antivirus flags on Windows (unsigned binary that simulates keystrokes) | Medium | Medium | Code signing (stage 9); keystroke simulation only on explicit user action |
| AGPL implications of MuPDF | Low | Low | GPLv3 section 13 allows the combination without relicensing (11); Poppler as the alternative |
| Scope creep before a usable release | High | Medium | Stage 3 is the first-release line; everything else waits |

---

## 14. Open questions

1. Final name and application ID (reverse-DNS, for example `io.github.<user>.legilo`). Check for clashes with existing projects.
   - **Decide before stage 3.** The ID ends up in KGlobalAccel's shortcut storage, the settings and data paths, the desktop files and the IPC address.
   - Changing it after the first release loses users' shortcuts and settings.
2. Hosting: GitHub or GitLab (affects CI configuration and where the apt repository and releases live).
3. ~~Default voice.~~ Resolved at stage 0: `en_GB-cori-medium` (12).
4. Should a speak-selection capture always open the reader, never, or only for long text? The provisional default is `ifVisible` (Appendix A).
5. Windows 10 support: best effort, or officially supported? Qt 6.12 is the last Qt release that supports it, and its open-source patch releases end when Qt 6.13 ships (around March 2027) (4.2).
6. PDF library (stage 10). The licence no longer depends on it (section 11).
7. ~~Qt version on Windows.~~ Resolved at stage 0: Qt 6.12 (4.2).

---

## 15. Future work

- Speech-to-text dictation: sherpa-onnx streaming or VAD-plus-offline models (Parakeet, Whisper, Moonshine).
  - Typing on Plasma Wayland via the RemoteDesktop portal and libei, with clipboard-and-paste as the fallback.
  - On Windows via `SendInput`.
- OCR for scanned PDFs (Tesseract).
- Skipping the references section in papers.
- More formats: EPUB, ODT, HTML, Markdown.
- More languages, with automatic language detection and voice choice.
- GPU acceleration (ONNX Runtime CUDA or DirectML; whisper.cpp with Vulkan for STT).
- Optional online engines.
- Windows 11 top-level context menu via `IExplorerCommand` and package identity.
- Flatpak (with the GlobalShortcuts portal or kglobalaccel access) and GNOME support.
- Pronunciation dictionary: user-defined replacements for names and acronyms, as text or as phonemes now that Legilo owns phonemisation (7.3.1).

---

## Appendix A: Settings keys

| Key | Type | Default | Notes |
|---|---|---|---|
| `general/autostart` | bool | false | |
| `general/notifyStart` | bool | true | |
| `general/notifyPause` | bool | true | |
| `general/notifySource` | bool | true | Says whether selection or clipboard was read. Error notifications have no key: they are always shown |
| `behaviour/speakWhileSpeaking` | enum | `stop` | `stop` or `restart` |
| `behaviour/selectionFallbackToClipboard` | bool | false | |
| `behaviour/showCaptureInReader` | enum | `ifVisible` | `never`, `ifVisible`, `always`, `longText` |
| `voice/default` | string | first installed | Voice ID |
| `voice/speed` | double | 1.0 | 0.5 to 2.5. The faster and slower actions change and save it (7.5) |
| `voice/segmentGapMs` | int | 120 | |
| `voice/paragraphGapMs` | int | 350 | |
| `voice/threads` | int | 0 | 0 means automatic |
| `voice/cacheSize` | int | 2 | Loaded models kept |
| `voice/unloadAfterIdleMin` | int | 0 | Unload voices after this many minutes without playback; 0 means never (7.3.2) |
| `voice/firstChunkChars` | int | 80 | Target length of the short first chunk (7.5) |
| `voice/rewindOnResume` | bool | false | Rewind to the start of the sentence on resume (7.4) |
| `cleanup/reflowMode` | enum | `auto` | `auto`, `always`, `never` |
| `cleanup/detectThreshold` | double | 1.0 | Advanced. The fitted weights are scaled so that 1 is the threshold (7.2.3) |
| `cleanup/pageNumbers` | bool | true | PDF mode |
| `cleanup/repeatedLines` | bool | true | PDF mode |
| `cleanup/dehyphenate` | bool | true | PDF mode |
| `cleanup/urls` | enum | `host` | `host`, `full`, `remove` |
| `cleanup/emails` | enum | `host` | `host`, `full` |
| `cleanup/citations` | enum | `pdfOnly` | `never`, `pdfOnly`, `always` |
| `cleanup/authorYearCitations` | bool | false | Remove author-year citations (7.2.5) |
| `cleanup/abbreviations` | string list | empty | Added to the built-in abbreviations that never end a sentence (7.2.7) |
| `cleanup/maxSegmentChars` | int | 300 | |
| `cleanup/detectInFiles` | bool | false | |
| `reader/followMode` | bool | true | |
| `reader/storeCapturePositions` | bool | false | |
| `ipc/allowSpeakText` | bool | false | Accept `speak-text` over IPC (7.8, 8) |
| `windows/excludedApps` | string list | empty | Simulated copy is never used in these executables (7.1.3) |

Shortcuts are stored by KGlobalAccel on Linux, and under `shortcuts/<id>` on Windows.

## Appendix B: Command-line reference

```
legilo                          start the tray app (or show the reader if already running)
legilo --tray                   start minimised to the tray (used by autostart)
legilo --speak-selection        read the selection
legilo --speak-clipboard        read the clipboard
legilo --speak-pdf-text         read the selection with PDF reflow forced on
legilo --speak-text TEXT        read TEXT (needs ipc/allowSpeakText)
legilo --pause                  pause or resume
legilo --stop                   stop
legilo --faster | --slower      change the speed by 0.1, now and as the saved default
legilo --voice ID               voice for this command only
legilo --speed X                speed for this command only
legilo --read [FILE...]         open files in the reader and start reading;
                                with no files, show the reader
legilo --show                   show the reader
legilo --clean-only [FILE]      print the spoken text (stdin if no file)
legilo --explain [FILE]         print detection features and score
legilo --list-voices            list installed voices
legilo --install-voice ARCHIVE  install a voice from a downloaded archive (7.3.2)
legilo --version | --help
```

What each command does when no instance is running is described in 7.8. `--clean-only`, `--explain`, `--list-voices` and `--install-voice` never need one.

Exit codes: 0 for success, 1 for an error reported by the instance, 2 for invalid arguments, 3 if an instance was needed but could not be started.

## Appendix C: Desktop integration files

`/usr/share/applications/io.github.example.legilo.desktop`:

```
[Desktop Entry]
Type=Application
Name=Legilo
GenericName=Text to Speech Reader
Comment=Read selected text, the clipboard or documents aloud
Exec=legilo --read %F
Icon=io.github.example.legilo
Terminal=false
Categories=Utility;Accessibility;Qt;KDE;
MimeType=text/plain;
Keywords=speech;tts;read;aloud;voice;
StartupNotify=false
```

Stage 10 adds `application/pdf;` to `MimeType`.

`~/.config/autostart/legilo.desktop` (written by the app):

```
[Desktop Entry]
Type=Application
Name=Legilo
Exec=legilo --tray
Icon=io.github.example.legilo
X-KDE-autostart-phase=2
```

`/usr/share/kio/servicemenus/legilo.desktop` (stage 10):

```
[Desktop Entry]
Type=Service
MimeType=application/pdf;text/plain;
Actions=readAloud;
X-KDE-Priority=TopLevel

[Desktop Action readAloud]
Name=Read aloud in Legilo
Icon=io.github.example.legilo
Exec=legilo --read %F
```

Windows "Open With" registration (per user; written by the installer):

```
HKCU\Software\Classes\Legilo.Text\shell\open\command
    (Default) = "C:\Users\<user>\AppData\Local\Programs\Legilo\legilo.exe" --read "%1"
HKCU\Software\Classes\.txt\OpenWithProgids
    Legilo.Text = ""
```

## Appendix D: Revision history

### Draft 0.19 (6 November 2026)

- Reflow keeps a joined word's two lines apart in PDF text, ends a paragraph after a sentence under 80% of the usual width and after a short heading without a stop, and counts clause numbers as list markers (7.2.4, 7.2.3).
- `TextCapture` lives in `core` (7.1.1). The clean-up pipeline and the pair test are built; four pairs cannot match from text alone and are expected failures with a paragraph floor (9.2, 12).
- `cleanup/detectThreshold` defaults to 1.0 (Appendix A).

### Draft 0.18 (6 November 2026)

- Reflow drops blank lines inside a sentence in PDF mode, and joins "word- word" inside a line where the word list settles it (7.2.4, rules 3 and 5; 12, stage 2 results).

### Draft 0.17 (6 November 2026)

- `wrap` needs at least three words on the line; weights refitted (7.2.3).
- Its held-out result, the layout-excerpt experiment that was dropped, and detection left at its current precision until stage 3 brings real selections (12, stage 2 results).

### Draft 0.16 (6 November 2026)

- PDF detection as built: blank lines ignored, `wrap` as the longest run of wrapped lines continued by a lowercase letter, `midline` and `symbols` added, `uniform` and `code` dropped, `list` slightly positive, weights fitted on the corpus and scaled to a threshold of 1 (7.2.3, 1.3).
- The corpus labels samples by what they hold, adds seven documents as a fresh held-out set and moves the spent one to tuning (9.2).
- Detection meets its recall target but not its precision target on the fresh set; the precision check is an expected failure until tables and equations inside PDFs are handled. More viewer findings, and the copy helper's guard (12, stage 2 results).

### Draft 0.15 (30 October 2026)

- Chromium's selections join the comparison: no blank lines, and hyphenated breaks inside a line, which reflow must handle; `pdftotext`'s blank lines are no more reliable as paragraph breaks than Firefox's (9.2, 12).

### Draft 0.14 (30 October 2026)

- The regression corpus is built and described (9.2).
- Real selections from Firefox, Okular and Chromium break the assumption that a blank line marks a paragraph, and Chromium brings hyphenated breaks inside a line; reflow and detection are to change accordingly (12, stage 2 results).
- Keyboard selections may not set the primary selection, which speak-selection reads (7.1.2).

### Draft 0.13 (30 October 2026)

Stage 2, first steps:

- The word list is SCOWL 2020.12.07 at the standard size, from Debian's `wbritish` and `wamerican`, with its notice in `debian/copyright` (5, 11).
- The segmenter's custom rules replace `QTextBoundaryFinder` rather than refine it (7.2.7).

### Draft 0.12 (30 October 2026)

- The Windows build moves from stage 1 to stage 5, so stage 1 closes on Linux (12).

### Draft 0.11 (30 October 2026)

- Stage 1 playback and the `--speak` commands are done; the exit criteria are assessed, and the Windows one is not met (12).

### Draft 0.10 (30 October 2026)

- Draft 0.9 blamed one-word sentences for Piper's babbling. Most of it came from sherpa-onnx's token sequence, which Legilo now patches; the one-word rule stays, for the babbling that remains in Piper's own runtime (7.2.7, 10.1, 12, 13).

### Draft 0.9 (30 October 2026)

Stage 1 listening checks on a real device (12):

- The speech path works end to end through PipeWire; pause and resume are clean, and unplugging headphones stops playback (7.4).
- One-word segments make Piper babble, so segmentation merges them (7.2.7).
- Kokoro's British speakers sound better with British phonemes, confirming the per-speaker `espeakVoice` (7.3.2).

### Draft 0.8 (29 October 2026)

Stage 1 audio:

- Flush requests reach the audio callback in order through their own queue, fixing a race where audio from before a seek survived a quick speed change (7.4, 12).
- miniaudio comes from Debian's `libminiaudio-dev` on Linux instead of a vendored copy (5, 6.5).
- Device loss handling is implemented, and the audio output has an integration test on miniaudio's null backend (7.4, 9.3).

### Draft 0.7 (29 October 2026)

Stage 1 installer:

- Extraction writes the files itself and accepts only regular files and directories, instead of relying on libarchive's `SECURE_*` flags. It caps the unpacked size and skips the models' espeak-ng data. Model IDs and the manifest's contents for archives not in the catalogue are described, and installs move to a worker in the model manager (7.10, 12).

### Draft 0.6 (29 October 2026)

Stage 1 TTS engine:

- Concurrent synthesis needs no lock; synthesis is not deterministic; speed is not proportional to duration (7.3.1, 12).
- Kokoro takes the Piper phoneme stream. The espeak-ng voice comes from `OfflineTts::Lang()`, and a speaker can override it (7.3.1, 7.3.2).
- `Voice` gains the model ID, file paths and `espeakVoice` and loses the sample rate; the `TtsEngine` interface and the manifest format are described (7.3.2).
- The TTS integration test exists, with pinned test voices (6.5, 9.3).

### Draft 0.5 (29 October 2026)

Stage 1 decisions on building sherpa-onnx:

- On Linux, sherpa-onnx links Debian's ONNX Runtime 1.21 rather than its prebuilt 1.28.2, after a comparison showed the same speed (1.3, 5, 10.1, 12, 13).
- sherpa-onnx is built as an `ExternalProject` instead of with `FetchContent`, and its ten source archives are pinned in `cmake/Dependencies.cmake` and fetched by `scripts/fetch-deps`. Check and package builds run without network (6.5, 10.1).
- `dh_shlibdeps` needs no `-l` for the private libraries; the official-archive note gives the new reason (10.3).
- The libraries compiled into sherpa-onnx and their licences are listed (5, 11).
- New section: Stage 1 results (12).

### Draft 0.4 (28 October 2026)

Stage 1 findings:

- The espeak-ng voice for British English is `en`. Legilo uses the system's espeak-ng data, which differs from piper-phonemize's in one American vowel (7.3.1, 13).

### Draft 0.3 (28 October 2026)

Stage 0 results (12) and the changes they require:

- sherpa-onnx 2.0 removed phonemisation, so Legilo gets its own `Phonemiser` on espeak-ng (5, 6.1, 6.4, 7.3.1, 9.1, 11, stage 1).
- Kokoro and Piper high are too slow for typical laptop CPUs. They are marked `needsFastCpu`, playback warns on underruns, and fp32 is preferred over int8 (NFR-01, 7.3.2, 7.10, 13).
- The default voice is `en_GB-cori-medium`. Voices derived from lessac are flagged for licence (7.3.2, 7.10, 11, stage 4).
- Windows uses Qt 6.12 (4.2).
- Open questions 3 and 7 are resolved; question 5 gains the Qt 6.12 constraint (14).
- Selection capture through `KSystemClipboard` works without focus on Plasma Wayland (12, 13).
- The repository layout gains `packaging/container/` and `scripts/`. The Debian package gains a generated manual page (6.5, 10.3).

### Draft 0.2 (27 October 2026)

Review pass. The main changes:

- **Design**
  - Speech rules now run per segment at synthesis, so the reader shows the source text and the preview shows the spoken form (7.2.1, 7.2.5, 7.2.8, 7.12).
  - A short first chunk keeps first audio within NFR-01 (7.5).
  - The cancellation granularity of sherpa-onnx is stated, and the stop test redefined to match (7.3.1, 9.3).
  - A flush protocol for the audio queue (7.4).
  - Export uses its own voice instance; loaded voices are held by shared pointer; concurrent synthesis is verified in stage 1 (7.3, 7.13).
  - Single-instance start-up uses a lock file and clears stale sockets (7.8).
  - Voices can be installed from stage 1 with `--install-voice` (FR-28, 7.3.2).
  - Resume positions use content-based IDs and text offsets, mapped through a diff when settings change (7.11.5, 7.12).
  - Losing the audio device pauses playback (NFR-06, 7.4, 8).
  - The `core` dependency rule is corrected, and `TtsEngine` and `AudioOutput` are named as interfaces (6.1).
  - New stage 0 spikes: selection capture on Wayland, voice availability and latency (12).
- **Corrections**
  - Voices directory uses `AppLocalDataLocation` (7.3.2).
  - Opus sample rates (7.13).
  - The MuPDF licence combination needs no relicensing (7.15.1, 11).
  - The Windows text-file fallback uses the system code page (7.14).
  - Windows 10 support status (4.2).
  - The Windows Qt version is now a stage 0 decision (4.2).
- **Text pipeline**
  - Confirmed de-hyphenation runs in all modes.
  - Table-safe page-number and repeated-line rules.
  - Abbreviation groups; URLs and emails as single tokens; recursive splitting; short-segment merging within paragraphs.
  - Superscripts moved to a speech rule.
  - Optional author-year citation removal.
  - Typed pipeline steps.
- **Shortcuts and accessibility**
  - Windows defaults avoid Ctrl+Alt, which is AltGr on many layouts (7.6.2).
  - Reader playback keys no longer take over caret movement (7.11.4).
  - Media controls through MPRIS and System Media Transport Controls (FR-29, 7.7).
- **Testing and CI**
  - Detection targets are precision and recall on a held-out split (9.2).
  - New unit-test areas (9.1).
  - Test-driven development is the default for `core` (9.5).
  - Benchmarks move to a separate, non-blocking job (9.6).
  - `clang-tidy` runs in CI (10.2).
- **Consistency**
  - `speak-text`, `--install-voice` and the behaviour with no running instance are specified (7.8, Appendix B).
  - `--read` without files is defined.
  - Notification and settings keys are completed (Appendix A).
  - Versioning, the changelog and the README come earlier (10.3, 12).
  - The application ID must be decided before stage 3 (14).
  - Other additions: catalogue signing (7.10), idle voice unloading (7.3.2), the Windows pipe name includes the session ID (7.8), and `IUIAutomation2` timeouts (7.1.3).

### Draft 0.1 (27 October 2026)

First draft.
