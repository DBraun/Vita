# Changelog

<!-- changelog-start -->

All notable changes to this project are documented here.

This project adheres to [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/)
and to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-07-27

### Added

- **Parallel rendering with threads.** `render`, `render_file`, `load_preset`,
  `load_json` and `to_json` now release the GIL, so Python threads that each own
  a separate `Synth` render on separate cores. See
  [Parallel rendering](https://dbraun.github.io/Vita/parallel.html) and the new
  `examples/multithreading_presets` example.
- `Synth.set_sample_rate` to configure the render sample rate ([#6]).
- Documentation site built with Sphinx and published to GitHub Pages, covering a
  quickstart, parallel rendering, and a generated API reference.
- This changelog.
- Wheels for CPython 3.14.
- Type stubs. Wheels now ship `vita.pyi` and `py.typed`, generated from the
  compiled module by nanobind's stub generator, so editors and type checkers
  understand the API.
- `tests/test_threading.py`, covering concurrent rendering, concurrent preset
  loading, shared-`Synth` serialization, and recovery from failed loads.
- `tests/test_control_options.py`, which reads the metadata of every control
  rather than a hand-picked few.

### Changed

- **Building from source is now just `pip install .`** on every platform.
  `setup.py` compiles the extension itself, deriving the Python paths from the
  interpreter running the build, so no environment variables have to be set
  beforehand.
- `ControlValue.set` and `Value.set` register their integer and enum overloads
  ahead of the float one. Runtime behaviour is unchanged -- nanobind tries every
  overload without implicit conversion first -- but the generated stubs no
  longer look like the float overload shadows the others.
- Upgraded nanobind from 2.7.0 to 2.13.0. Among other things this fixes an extra
  copy of the return value when a `call_guard` is in use, and speeds up handing
  NumPy arrays back to Python.
- `FFT::transform()` now returns a thread-local `FourierTransform`. The IPP and
  kissfft backends keep mutable scratch state in the object, which concurrent
  wavetable work would have corrupted.
- `RandomGenerator::next_seed_` is now `std::atomic<int>`, so constructing
  synths on several threads at once is not a data race.
- Docstrings across the bindings were filled in and corrected; `render`
  previously described itself as writing a file and returning a bool.
- CI builds and tests against manylinux_2_28 rather than manylinux2014, and
  tests the built wheel rather than a stray extension module in the source tree.

### Fixed

- The oscillator and sample `destination` controls accepted one value more than
  there are routings. Vital sizes them as `kNumSourceDestinations + kNumEffects`
  where every comparable parameter uses `count - 1`, so the trailing value named
  no destination and silently routed the source nowhere; `set_normalized(1.0)`
  landed on it. Their `max` is now 13 rather than 14, so the full normalized
  range maps onto real routings. Presets are unaffected -- they store raw
  values, and nothing clamps on load.
- `get_control_details(...).options` and `get_control_text` no longer read past
  the end of a control's name table. Several controls accept more values than
  Vital has names for -- `view_2d` and `view_spectrogram` span 0-2 with two
  names, `filter_*_style` spans 0-9 with five -- and the count was taken from
  the value range rather than the table. In 0.0.5 reading `.options` on any of
  those seven controls **crashed the interpreter**; four more
  (`osc_{1,2,3}_destination`, `sample_destination`) silently reported a
  fifteenth option, `'Shelf'`, that does not exist. Both now stop at the last
  real name, so a control's `options` may be shorter than `max - min + 1` when
  the surplus values are unnamed.
- A failed `load_json` or `load_preset` no longer leaves the synth's critical
  section held. Because rendering releases the GIL, a subsequent `render` would
  have hung uninterruptibly instead of raising.
- The render buffer is no longer leaked if an exception unwinds out of the DSP
  loop.
- The modulation source and destination caches are now initialized through a
  thread-safe function-local static instead of an unsynchronized flag.
- `examples/multiprocessing_presets` passed MIDI velocity as `100` where the API
  expects 0-1, and ignored its own `--render-duration` argument.

### Removed

- `build_linux.sh` and `build_macos.sh`, and the separate msbuild step in CI.
  All three are now handled by `setup.py`.
- `DistortionType.None`, renamed to `DistortionType.Off`. **Breaking**, though
  only nominally: `None` is a Python keyword, so `DistortionType.None` was a
  syntax error and the member could only ever be reached through
  `getattr(DistortionType, "None")`.
- macOS x86-64 wheels, so **Intel Macs are no longer supported**. The
  `macos-13` runner image is retired and those CI jobs no longer start. There is
  no source distribution to fall back on, so `pip install vita` fails with "no
  matching distribution found" on an Intel Mac; stay on 0.0.5 there. Apple
  silicon, Linux x86-64 and Windows x86-64 are unaffected.
- `librosa` from the test requirements; nothing used it, and it was the main
  source of dependency-resolution failures on new Python versions.

## [0.0.5] - 2025-06-03

### Added

- Normalized parameter access via `ControlValue.set_normalized` and
  `get_normalized`, plus parameter introspection through
  `Synth.get_control_details` and `Synth.get_control_text` ([#3]).

## [0.0.4] - 2025-02-13

### Added

- Pickle support for `Synth`, which makes it usable across `multiprocessing`
  workers ([#1]).

## [0.0.3] - 2025-01-08

### Fixed

- Wheel-building workflow corrections.

## [0.0.2] - 2025-01-08

### Fixed

- Wheel-building workflow corrections.

## [0.0.1] - 2025-01-08

### Added

- Initial release: Python bindings for the Vital synthesizer, covering preset
  loading and saving, control access, modulation routing, and rendering to a
  NumPy array or a wav file.

[Unreleased]: https://github.com/DBraun/Vita/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/DBraun/Vita/compare/v0.0.5...v0.1.0
[0.0.5]: https://github.com/DBraun/Vita/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/DBraun/Vita/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/DBraun/Vita/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/DBraun/Vita/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/DBraun/Vita/releases/tag/v0.0.1
[#1]: https://github.com/DBraun/Vita/pull/1
[#3]: https://github.com/DBraun/Vita/pull/3
[#6]: https://github.com/DBraun/Vita/pull/6
