# TabPlayer bootstrap audit — 2026-10-12

## Baseline
Main includes merged PR #1 (bootstrap verification README update). Reviewed `CMakeLists.txt`, `docs/ENGINEERING_SPEC.md`, playback engine, processor, playback tests and Linux CI. Full-tree static audit and runtime QA remain incomplete. No native build was executed in this environment because direct GitHub cloning is unavailable. The Windows pipeline added on this branch must be validated by GitHub Actions before release.

## Confirmed issues
1. **Critical: hanging MIDI notes on stop, seek, rewind, and score replacement.** `TabPlaybackEngine::stop`, `seekBeat`, `rewind`, `clear`, and `setScore` only modify atomics. `process` returns without issuing note-off, sustain-off, or controller reset messages when stopped. A note-on previously emitted can remain sounding indefinitely.
2. **High: unsynchronized mutable score read.** `TabPlayerAudioProcessor::processTyped` accesses `score.getMeta().tempoBpm` while `loadTabText` can replace `score` from the UI thread. This is a potential data race. Publish the tempo with the immutable playback snapshot or an atomic.
3. **High: lifetime/RT guarantee not met.** `getTimeline` uses atomic `shared_ptr` load in the audio callback, and the callback can release the last reference, potentially destructing the timeline on the real-time thread. Migrate to explicit non-RT retirement.
4. **High: playback does not follow host PPQ/play-stop/seek.** Only host BPM is read, meaning a DAW session can be stopped while TabPlayer keeps generating MIDI on its independent timeline.
5. **High: time complexity of the audio callback.** Every `process` scans events from the beginning, even late in long scores. Use a bounded cursor or binary search, with proper invalidation on seek, tempo changes, and timeline replacement.
6. **High: unvalidated parsed MIDI values.** `compile` clamps velocity/channel but passes `note.midiNote` directly to MIDI construction. Invalid source data should be rejected or clamped to 0..127 after preserving import diagnostics.
7. **Medium: only track 0 compiled.** `compile` reads `score.getTrack(0)`, dropping other tracks.
8. **Medium: mismatched plugin/standalone expectations.** `IS_MIDI_EFFECT TRUE` and `BusesProperties()` do not implement the embedded instrument hosting described in the product specification. Confirm expected routing in real DAWs before promising standalone audio.
9. **Medium: only one smoke test.** Missing assertions for note-off ordering, stop/seek cleanup, invalid data, overlapping notes, alternate meters, state round-trips, concurrency, and large files.
10. **Release blocker: no Windows CI previously existed.** Windows build/test/artifact verification introduced on this branch.

## Proposed release gates
- Windows CI builds both requested targets and passes CTest.
- Audio-thread MIDI state reset tests pass for stop, seek, rewind, score replacement, end-of-score and repeated play.
- ThreadSanitizer or equivalent race analysis on relevant Linux harnesses.
- Test sample rates 44.1/48/96 kHz, varied blocks, and transport discontinuities.
- Confirm VST3 MIDI routing in REAPER and FL Studio; validate plugin with pluginval.
- Verify clean startup, file import, state save/load, playback and removal/reload.
- Only then publish a versioned beta prerelease with binaries, hashes, known limitations, and setup instructions.

## Outstanding features in engineering spec
Expression/technique mapping, host-sync, JSON instrument profiles, loops/count-in, structured GP and MusicXML import, project format, hosted instrument loading, GUI interactions, and real-time ownership hardening are not established as complete by the inspected implementation.
