# TabPlayer Engineering Specification

Status: bootstrap implementation + product specification
Product: TabPlayer
Target: VST3 MIDI-effect / MIDI-generator plug-in first, standalone companion second
Primary use: load tablature, display it, play it in time, and drive a software instrument with expressive MIDI

## 1. Product definition

TabPlayer is a tablature performance engine. It is not a guitar synthesizer and it is not a Luthier edition. Its job is to turn tablature into an editable, inspectable performance timeline and then tell another instrument what to play, when to play it, and how to articulate it.

The plug-in therefore behaves more like a sequencer or MIDI effect than an audio effect:

Tab file -> parser -> normalized score -> performance compiler -> transport -> expressive MIDI -> instrument

The same engine must also support an embedded instrument-host mode so a user can load a compatible instrument plug-in inside TabPlayer where the host/platform permits it. External MIDI routing remains the canonical architecture because it composes cleanly in every DAW.

## 2. Non-goals

TabPlayer must not contain Luthier DSP, guitar models, presets, licensing, cabinet/amp/effects code, workshop code, Luthier UI, Luthier parameter IDs, or any Luthier host state.

The Luthier repository is a source reference only. No TabPlayer work is written back to Luthier.

## 3. Initial extracted engine

The bootstrap imports only the reusable functional tab path from Luthier:

- PerformanceScore data model
- resilient ASCII tab reader
- whole-document normalizer
- dialect lexer
- directive applier
- semantic adapter
- technique compiler
- diagnostics model

The extracted code is moved into the `tabplayer` namespace and detached from `DspCommon.h`. A local `TabPlayerTypes.h` owns shared limits.

The Luthier-only `TabPlaybackTuningSession` is not copied as a dependency because it mutates `LuthierEngine` and `TuningEngine`. Its product behavior is represented in TabPlayer by score tuning metadata, MIDI pitch assignment, per-string channels, and a future instrument-adaptation policy.

## 4. Supported source formats

### 4.1 Phase 1 required
- ASCII `.tab`
- text `.txt`
- common string labels
- standard and named alternate tunings
- explicit note-list tunings
- capo
- tempo
- time signature
- repeats
- common ASCII techniques already understood by the extracted engine

### 4.2 Phase 2
- Guitar Pro GP3/GP4/GP5
- GPX/GP6
- GP7/GP8/GPIF
- MusicXML
- MIDI import as a non-tab performance source

Structured formats must converge into the same PerformanceScore contract. Format-specific behavior must not leak into playback.

## 5. Canonical score model

Every playable note carries:
- absolute musical time derived from measure + beat
- duration
- string index
- fret
- MIDI note
- pitch in Hz
- velocity
- technique list
- tie state
- source provenance where available

Every track carries:
- instrument role/name
- string count
- open-string tuning, high string first
- capo
- measures

Every measure carries:
- time signature
- optional tempo change
- section name
- chord symbols
- voices

The score is immutable to the real-time playback path after compilation.

## 6. Playback compiler

The playback compiler converts PerformanceScore into an immutable, time-sorted event timeline.

Minimum event types:
- note on
- note off
- pitch bend
- channel pressure / CC expression
- articulation keyswitch
- sustain/hold
- program/preset request metadata
- transport marker

Phase 1 bootstrap emits note on/off events. Every tab string receives its own MIDI channel where possible so later pitch/technique expression can remain independent between strings.

Ordering rule at an identical timestamp:
1. note-offs
2. controller resets
3. pitch setup
4. note-ons
5. expression events

This prevents stuck notes and note retrigger ambiguity.

## 7. Technique mapping

The engine must map score techniques into instrument-neutral performance intents first. Instrument-specific MIDI mappings are a second layer.

Canonical technique intents include:
- bend / bend release / pre-bend
- slide up/down
- legato slide
- hammer-on
- pull-off
- palm mute
- dead note
- natural / pinch / artificial / tap harmonic
- tap
- vibrato
- trill
- whammy
- ghost note
- accent
- staccato
- let ring
- pick stroke up/down
- slap
- pop

An Instrument Profile maps these intents to one or more:
- MPE pitch bend
- poly/channel pressure
- CC
- MIDI 2.0 per-note controller when available
- keyswitch
- program change
- velocity transform
- duration transform
- note overlap/legato rule

Unknown mappings must degrade to the base note rather than block playback.

## 8. Instrument profiles

Each profile contains:
- name
- target plug-in/vendor identity hints
- channel mode: single, per-string, MPE lower-zone, custom
- pitch-bend range
- keyswitch table
- CC table
- articulation priority
- legato overlap rule
- palm-mute duration/velocity treatment
- vibrato policy
- slide policy
- reset messages sent on stop/seek
- optional factory preset name or program

Profiles are user-editable JSON and are independent of a specific tab.

Factory profile families:
- Generic MIDI
- Generic MPE
- guitar sampler
- bass sampler
- monophonic lead
- keyswitch orchestral-style instrument

## 9. Instrument loading

Two operating modes are required.

### 9.1 DAW routing mode
TabPlayer produces MIDI and the DAW routes it to another instrument. This is the default and most portable mode.

### 9.2 Embedded instrument mode
TabPlayer may host an instrument plug-in internally using JUCE AudioPluginFormatManager / AudioPluginInstance on supported platforms.

Requirements:
- scan only on user request
- cache scan results outside the audio thread
- never instantiate or destroy a plug-in on the audio thread
- process hosted instrument audio after TabPlayer MIDI generation
- expose a single stereo output initially
- support bypass/unload
- persist plug-in identifier and state
- recover cleanly when the instrument is missing
- sandbox/crash isolation is a later hardening item

Embedded hosting must not be required for the core product to function.

## 10. Transport

Controls:
- play
- pause
- stop
- rewind
- seek
- loop region
- speed 25% to 200% in UI, engine range up to 400%
- count-in
- metronome
- follow host transport
- independent transport

Modes:
- Host Sync: host PPQ/tempo is authoritative
- Free Run: TabPlayer tempo map is authoritative
- Practice: independent speed multiplier with optional host-free transport

Seeking must send an all-notes-off/reset bundle before starting from the new position.

## 11. Visual tab surface

The tab display is a primary feature, not a debug view.

Required behavior:
- string lines use guitar-tab orientation, highest string at top
- fret numbers rendered at exact score positions
- currently sounding notes highlighted
- playhead visible
- automatic horizontal follow
- manual horizontal scroll
- zoom from phrase view to multi-bar overview
- bar lines, measure numbers, sections
- tuning and capo header
- chord symbols
- technique glyphs
- loop selection
- click note to audition
- click/drag playhead to seek

Color must not be the only state indicator. Active notes use color plus weight/shape change.

## 12. Performance visualization

A lower or optional inspector shows the command being sent to the instrument in real time:
- note name / MIDI number
- string and fret
- velocity
- duration target
- active articulation
- outgoing channel
- pitch bend in semitones/cents
- CC/keyswitch messages
- current beat / bar / tempo

This makes TabPlayer useful as a learning/debugging tool for MIDI articulation as well as tab playback.

## 13. Editing

Phase 2 editor operations:
- move note
- change fret/string while preserving or intentionally changing pitch
- duration
- velocity
- add/remove techniques
- quantize
- humanize
- transpose
- string remap
- tuning override
- capo override
- repeat/loop edit

All edits operate on the canonical score. Source-text round-trip preservation is optional; exported TabPlayer project files are authoritative for edited sessions.

## 14. Project format

Extension: `.tabplayer`

Container data:
- schema version
- original source text/file metadata
- canonical score
- import diagnostics
- user edits
- transport settings
- loop regions
- instrument profile
- optional embedded instrument identifier/state
- UI view state

The format must be forward-compatible:
- unknown fields ignored
- schema migrations explicit
- no raw pointer or platform path assumptions

## 15. Plug-in state

VST state stores enough to reopen the exact working session:
- source text or project state
- current score/project version
- speed
- loop
- transport position
- instrument profile
- embedded instrument state if used

External source files are references only when the session also contains an embedded recovery copy.

## 16. Real-time rules

Audio/process thread:
- no file I/O
- no plug-in scanning
- no parser work
- no JSON
- no unbounded allocation
- no blocking locks
- no UI calls

Message/worker thread:
- file loading
- parsing
- normalization
- compilation
- instrument scanning
- project save/load

The compiled timeline is immutable. Publication to the audio thread must eventually use an RCU/double-buffer strategy whose retirement cannot free memory on the audio thread. The bootstrap uses an atomic shared timeline and is intentionally marked for hardening before 1.0.

## 17. MIDI safety

On stop, seek, project replace, instrument replace, and transport discontinuity send:
- note-off for tracked sounding notes
- CC 123 All Notes Off where appropriate
- sustain off
- pitch bend center
- articulation reset messages from the active profile

Repeated starts/stops must not leave hanging notes.

## 18. Host compatibility

Primary:
- Windows VST3
- macOS VST3/AU after macOS CI is added
- Linux VST3 for development/compatible hosts

Standalone:
- Windows/macOS/Linux where JUCE supports required audio/MIDI devices

CLAP is a planned format once the project has a stable CLAP integration strategy.

DAW validation matrix should include at least:
- FL Studio
- REAPER
- Ableton Live
- Bitwig
- Studio One
- Cubase/Nuendo where available

Because MIDI-effect routing differs by host, documentation must include exact routing patterns per host.

## 19. UI layout

Header:
- Load
- project/title
- tuning/capo
- transport
- speed
- loop
- sync mode

Main:
- visual tab viewport

Bottom drawer:
- Performance
- Instrument
- Mapping
- Import diagnostics

Instrument tab:
- output mode
- target channel mode
- profile
- embedded instrument slot
- articulation monitor

## 20. Error handling

Import errors must be specific and non-destructive:
- previous score remains loaded if a new import fails
- diagnostics report partial recovery
- conflicting tuning is shown, not silently guessed
- unsupported file format identifies the format
- missing embedded instrument preserves the project and allows rerouting

## 21. Testing strategy

Unit:
- parser/normalizer corpus
- score invariants
- timeline ordering
- same-time note-off before note-on
- velocity bounds
- per-string channel assignment
- seek/stop resets
- technique intent mapping
- instrument-profile translation

Property/fuzz:
- arbitrary ASCII input never crashes
- bounded input sizes
- malformed tuning/repeat constructs
- Unicode and line ending variations

Integration:
- known tab -> expected score
- score -> expected MIDI event trace
- project state round-trip
- host transport discontinuities

Plug-in:
- pluginval
- DAW smoke tests
- no MIDI when stopped
- deterministic playback at fixed tempo

Performance:
- large tab compilation time
- process-block CPU budget
- zero parser work on audio thread
- allocation instrumentation on process path

## 22. Security and robustness

Treat tab files and imported metadata as untrusted input.
- enforce size/line/system limits
- sanitize project strings before display where required
- never execute directives
- embedded plug-ins are user-selected binaries and must never be auto-loaded from a tab file
- no network access is needed for core playback

## 23. Bootstrap implementation in this repository

The first implementation includes:
- extracted independent ASCII-tab import engine
- canonical score
- immutable compiled note timeline
- per-string MIDI channel output
- VST3 MIDI-effect target
- standalone target
- load button
- play/stop/rewind
- speed control
- visual tab view
- live playhead
- active-note highlighting
- minimal playback regression test
- Linux CI build/test workflow

## 24. Next implementation order

1. Make bootstrap CI green and fix portability/build issues.
2. Add note-state tracking and guaranteed reset bundle on stop/seek.
3. Add host-sync PPQ positioning and transport discontinuity handling.
4. Add technique intent compiler and pitch-bend/MPE output.
5. Add instrument-profile JSON and generic profiles.
6. Add loop/count-in/metronome/practice controls.
7. Add full tab visual glyphs and interactive seeking.
8. Add structured Guitar Pro/MusicXML import adapters.
9. Add project format/state migrations.
10. Add embedded instrument hosting.
11. Add pluginval and multi-platform CI.
12. Performance hardening: retire atomic shared_ptr from process path and verify zero allocations.
