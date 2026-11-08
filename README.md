# Legilo

Legilo is an offline text-to-speech reader for KDE Plasma and Windows. It
reads the selected text, the clipboard, or a plain-text file aloud with
neural voices (Piper and Kokoro, through sherpa-onnx) that run entirely on
your machine. Text copied from PDFs is cleaned up before it is spoken.

**Status:** early development. Stage 3 of the delivery plan, the tray
app, is in progress. Legilo runs in the system tray and reads the
selection or the clipboard aloud when you press its global shortcuts:

| Shortcut | Action |
|---|---|
| Meta+Alt+S | Speak the selection; press again to stop |
| Meta+Alt+C | Speak the clipboard |
| Meta+Alt+P | Pause or resume |
| Meta+Alt+X | Stop |

They can be changed in System Settings > Keyboard > Shortcuts, under
Legilo. A default that another application already uses is left unset.
There is no settings dialog or reader window yet.

From the command line:

```
legilo                              # start in the tray
legilo --speak-selection            # also --speak-clipboard, --pause, --stop
legilo --install-voice vits-piper-en_GB-cori-medium.tar.bz2
legilo --speak "Hello from Legilo."
legilo --speak-file notes.txt --speed 1.2
legilo --clean-only copied.txt      # the text as it would be spoken
legilo --explain copied.txt         # why it was or was not taken for PDF text
```

The speak, pause and stop commands go to the running instance, starting
one if needed. `--clean-only` and `--explain` read standard input when
given no file.

The design, requirements and staged plan are in
[docs/legilo-tdd.md](docs/legilo-tdd.md).

## Building on Debian 13 (trixie)

Install the build dependencies:

```
sudo apt install build-essential cmake ninja-build pkgconf qt6-base-dev \
    libespeak-ng-dev libonnxruntime-dev libarchive-dev libminiaudio-dev \
    libkf6globalaccel-dev libkf6guiaddons-dev \
    wbritish wamerican clang-format clang-tidy help2man
```

`debian/control` holds the authoritative list of build dependencies.

Download the pinned source archives the build needs (sherpa-onnx and the
libraries it builds with) into `third_party/downloads/`. The build itself
never downloads anything, so run this again whenever
`cmake/Dependencies.cmake` changes:

```
scripts/fetch-deps
```

The integration tests (`tests/integration/`) synthesise speech with real
voices, about 390 MB, and skip any voice that is absent. Fetch them with
`scripts/fetch-deps --test-voices`. Run the unit tests alone with
`ctest --preset linux-debug -LE integration`.

Configure, build and test with the CMake presets:

```
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Run the result with `build/linux-debug/src/app/legilo --help`.

Presets also exist for `linux-release`, `windows-debug` and
`windows-release`. The Windows build is not set up yet.

## Building in the trixie container

Builds that count (CI, packages) happen in a `debian:trixie` container, so
nothing installed on the host can leak into them. You need Podman, or
Docker with `CONTAINER_ENGINE=docker`.

| Command | What it does |
|---|---|
| `scripts/check` | Formatting, a warnings-as-errors build, the unit and integration tests, and clang-tidy. Run this before committing. |
| `scripts/bench` | Times the text pipeline on 5 MB of text in a release build, records the results in `build/linux-release/bench-results.tsv`, and reports measures over budget or more than 20% slower than the previous run (runs on the host) |
| `scripts/build-deb` | Builds the `.deb`, runs lintian on it, and writes the results to `out/` |
| `scripts/container [COMMAND]` | Runs a command (or a shell) in the container, with the repository at `/src` |
| `scripts/fetch-deps` | Downloads the pinned source archives into `third_party/downloads/` (runs on the host) |

`scripts/check` (with `--test-voices`) and `scripts/build-deb` run
`scripts/fetch-deps` first, then
build with the container's network switched off, as Debian's build daemons
do. The first run of the container scripts builds the container image, and
the first build compiles sherpa-onnx; each takes a few minutes.

## Repository layout

| Path | Contents |
|---|---|
| `src/core/` | Platform-independent logic: text processing, speech, audio, playback |
| `src/platform/` | Interfaces to the operating system, with Linux and Windows implementations |
| `src/app/` | The application: tray, windows, settings, command line |
| `tests/unit/` | Qt Test unit tests, run through CTest |
| `tests/integration/` | Tests against real dependencies, such as speech synthesis with real voices |
| `tests/corpus/` | The regression corpus for PDF detection and clean-up, and the tools that make it (its own README) |
| `tests/benchmark/` | Benchmarks, run by `scripts/bench` rather than CTest |
| `debian/` | Debian packaging |
| `packaging/` | The build container and, later, the Windows installer |
| `scripts/` | Developer scripts (see above) |
| `cmake/` | CMake modules, including the pinned dependency list (`Dependencies.cmake`) |
| `third_party/downloads/` | Archives from `scripts/fetch-deps` (not committed) |
| `third_party/test-voices/` | Voices from `scripts/fetch-deps --test-voices` (not committed) |
| `docs/` | Design documentation |

## Contributing

- C++20 and Qt 6. Shared code may use only the Qt 6.8 API, because that is
  what Debian trixie ships.
- Format with the repository's `.clang-format` before committing, and make
  sure `scripts/check` passes.
- New logic in `src/core` is written test-first.
- Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/).

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
