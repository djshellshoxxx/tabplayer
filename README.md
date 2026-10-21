# TabPlayer

TabPlayer is an independent tablature-to-MIDI performance engine for musicians. It imports tablature, builds a timed performance timeline, displays the score, and is intended to drive external software instruments through MIDI. It is **not** a guitar synthesizer or a Luthier edition.

## Development status

**Bootstrap / early development.** The repository includes an extracted ASCII-tab import engine, MIDI playback timeline, VST3/Standalone shell, and visual tab view. This does not establish production readiness, DAW compatibility, or availability of downloadable installers. Check [GitHub Releases](../../releases) for published binaries.

## Intended workflow

1. Import a supported tablature file (the initial implementation focuses on ASCII tab).
2. Inspect the parsed notes and diagnostics in the tab view.
3. Set up playback and route the generated MIDI to an instrument in your host, where supported.
4. Play, pause, and inspect timing and articulation.

Instrument hosting inside TabPlayer and broader tablature-format support are design goals, **not confirmed implemented features**.

## Architecture and documentation

The pipeline is: tab file → parser → normalized score → performance compiler → transport → MIDI output. The detailed scope, component boundaries, limitations, and future work are in [the engineering specification](docs/ENGINEERING_SPEC.md).

The reusable tab engine originated from Luthier. TabPlayer has its own namespace and shared types; the Luthier repository is not modified by TabPlayer development.

## Building and testing

This project uses a C++/JUCE plugin architecture. Consult the repository's build configuration and engineering specification for current dependencies and targets before building. Build commands and platform support should be verified against CI before being advertised as supported releases.

## Project status and support

This is development software. Report reproducible parser, timing, or MIDI routing issues using GitHub Issues, including a minimal tab example and host/platform details. Do not assume unverified formats or hosts are supported.
