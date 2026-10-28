# Legilo

Legilo is an offline text-to-speech reader for KDE Plasma and Windows. It
reads the selected text, the clipboard, or a plain-text file aloud with
neural voices (Piper and Kokoro, through sherpa-onnx) that run entirely on
your machine. Text copied from PDFs is cleaned up before it is spoken.

**Status:** early development (stage 0 of the delivery plan). The binary
builds, installs and prints its version, but does not speak yet.

The design, requirements and staged plan are in
[docs/legilo-tdd.md](docs/legilo-tdd.md).

## Building on Debian 13 (trixie)

Install the build dependencies:

```
sudo apt install build-essential cmake ninja-build qt6-base-dev \
    clang-format clang-tidy help2man
```

`debian/control` holds the authoritative list of build dependencies.

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
| `scripts/check` | Formatting, a warnings-as-errors build, the unit tests and clang-tidy. Run this before committing. |
| `scripts/build-deb` | Builds the `.deb`, runs lintian on it, and writes the results to `out/` |
| `scripts/container [COMMAND]` | Runs a command (or a shell) in the container, with the repository at `/src` |

The first run of any of these builds the container image, which takes a
few minutes.

## Repository layout

| Path | Contents |
|---|---|
| `src/core/` | Platform-independent logic: text processing, speech, audio, playback |
| `src/platform/` | Interfaces to the operating system, with Linux and Windows implementations |
| `src/app/` | The application: tray, windows, settings, command line |
| `tests/unit/` | Qt Test unit tests, run through CTest |
| `debian/` | Debian packaging |
| `packaging/` | The build container and, later, the Windows installer |
| `scripts/` | Developer scripts (see above) |
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
