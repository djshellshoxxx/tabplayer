# TabPlayer Feature Specification (implementation tracking)

Companion to `ENGINEERING_SPEC.md`. Maps each requirement to a module, an acceptance test and its status.

| Area | Module | Acceptance test | Status |
|---|---|---|---|
| Reset bundle on stop/seek/loop/replace (spec 17) | `playback/TabPlaybackEngine` | `PlaybackEngineTests` | implemented |
| Technique -> MIDI (bend, slide, vibrato, palm mute, legato, ghost, accent, staccato, dead) (spec 7) | `playback/TechniqueCompiler` | `TechniqueCompilerTests` | implemented |
| Channel modes single / per-string / MPE (spec 8) | `TechniqueCompiler` + `profile/InstrumentProfile` | `TechniqueCompilerTests` | implemented |
| Instrument profiles (JSON, factory set) (spec 8) | `profile/` | `InstrumentProfileTests` | implemented |
| Loop region, count-in, metronome, speed 0.1x-4x (spec 10) | `TabPlaybackEngine` | `TransportTests` | implemented |
| Host PPQ sync + discontinuity handling (spec 10) | `TabPlaybackEngine::process(hostPpq)` | `TransportTests` | implemented |
| `.tabplayer` project format + migration (spec 14/15) | `project/ProjectFile` | `ProjectFileTests` | implemented |
| Tab view: glyphs, zoom, click-seek, loop drag, chords, sections (spec 11) | `plugin/PluginEditor`, `plugin/TabView` | manual + build | implemented |
| Inspector: outgoing MIDI monitor (spec 12) | `plugin/PluginEditor` | manual | implemented |
| MIDI file export | `tools/MidiExporter` | `ScoreToolsTests` | added feature |
| Transpose / humanize / quantize | `tools/ScoreTransforms` | `ScoreToolsTests` | added feature |
| Chord detection | `tools/ScoreTransforms` | `ScoreToolsTests` | added feature |

## Not implemented (explicit)
- Guitar Pro / MusicXML import (spec 4.2, phase 2). Import adapters must converge on `PerformanceScore`.
- Embedded instrument hosting (spec 9.2). DAW routing is the supported mode.
- Note editing UI (spec 13). Score-level transforms exist as library functions.
- AU, CLAP, pluginval and DAW matrix validation (spec 18, 21).
- Audio-thread hardening: the timeline is published with `std::atomic_load` of a `shared_ptr` (spec 16); retirement can still free memory on the audio thread.

## Release
Tags `v*` trigger `.github/workflows/ci.yml`: Linux, Windows and macOS (universal) build, test, package (`shared/ci/package.py`) and publish a GitHub Release with `SHA256SUMS.txt`. Binaries are unsigned; macOS needs right-click Open, Windows shows SmartScreen.
