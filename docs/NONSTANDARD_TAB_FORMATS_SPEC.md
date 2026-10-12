# Nonstandard Tablature Compatibility — Research and Implementation Specification
Status: research-backed proposal; NOT implemented
Date: 2026-10-12

## Purpose
Expand TabPlayer beyond six-string ASCII guitar tabs. All importers emit one versioned, validated intermediate score, then compile via the common performance/MIDI engine. Never modify Luthier.

## Evidence and existing implementations
- alphaTab documented importers: Guitar Pro 3–5 binary, GP6 GPX, GP7/8 GPIF archives, MusicXML, CapXML and alphaTex. https://docs.alphatab.net/docs/introduction
- GP6 GPX is a proprietary container around GPIF XML, **not** a generic ZIP. https://www.demos.alphatab.net/docs/formats/guitar-pro-6
- GP7 is ZIP containing Content/score.gpif. https://www.demos.alphatab.net/docs/formats/guitar-pro-7
- GP8 GPIF may include extra audio files. https://alphatab.net/docs/formats/guitar-pro-8/
- ABC notation defines meter, key, accidentals, duration and repeats as symbolic notation; is not inherently fret tablature. https://abcwiki.org/abc%3Asyntax
- Freeform ASCII tabs have no single precise grammar; timing and layout can be ambiguous. https://en.wikipedia.org/wiki/ASCII_tab
- Instrument-specific layouts exist for mandolin, banjo, ukulele, dulcimer, tin whistle, harmonica and other instruments. https://tradlessons.com/

## Design constraints
1. Importer API: FormatProbe (bounded bytes) -> ImportDocument -> Diagnostics + CanonicalScore + OriginalNotation + Provenance.
2. Canonical Note includes absolute rational beat/duration, instrument ID, track, voice, pitch, optional string/course/fret, articulation intents, confidence, and original token span; explicit rests and tempo-map markers.
3. Instrument definitions are declarative (string/course tuning, reentrant string geometry, capo and partial capo, fret spacing, percussion mapping, harmonica layout and breath direction). Avoid hardcoding six strings.
4. Unsupported tokens preserve their source text, precise position and reason. Strict mode rejects underdetermined timing or pitch. Assisted mode prompts for assumptions and records them; no silent guesses.
5. MIDI compiler must handle simultaneous same-pitch note ownership on one channel and independent polyphonic voices; drum maps use channel 10 by policy configurable in instrument profile.
6. Resource limits: file size, decompression ratio and count, XML expansion, nesting depth, line length, note counts, repeat count, time horizon. Never execute imported metadata.
7. Parsers and format detection run off the audio thread. Immutable compiled playback snapshots, explicit non-audio-thread retirement.
8. Licensing: evaluate alphaTab's current license and integration options before vendoring/linking. Adapter/conversion architecture is preferred to copying code blindly.

## Priority / implementation modules
### P0: ASCII Dialect Detector
Recognition modules: labeled and unlabeled lines, reverse string order, lowercase/uppercase/tuning aliases, 4/5/6/7/8/9/12-string, 3/4/5-string instruments, tab separated columns, inconsistent spacing, CRLF/LF, optional tempo headers, repeats, odd meters, multiline wrapped bars, chord lyric lines, omitted string lines. Do not equate character columns with beats unless timing is explicitly defined or selected by user.

### P0: Structured Format Bridge
Adapters for GP3-5, GPX/GP6, GP7-8/GPIF, uncompressed MusicXML and compressed MXL. Evaluate alphaTab's parsing coverage and rights. Preserve timing, voices, grace notes, rests, ties, repeats, tuning, bends, slides, hammer-on, pull-off and palm-mute. Distinguish 'parsed', 'rendered', and 'played' feature support.

### P1: Percussion / Drum ASCII
Instrument lane labels (kick, snare, hi-hat, crash, ride, toms), symbols x/o/f/g/d, ghost notes, open/closed hi-hat, flams, rolls, chokes, triplets and swing. Map a lane+symbol to a configurable MIDI note and articulation; collision diagnostics when symbols overlap.

### P1: Non-guitar Fretted / Course Instruments
Ukulele (reentrant GCEA), mandolin (paired courses GDAE), banjo (short fifth string), mountain dulcimer (diatonic frets and 6½ fret), bass (4/5/6 strings), 7/8/9-string guitar and microtonal fret offsets. Distinguish course from individual strings; partial capos and uneven string start positions.

### P1: Harmonica and Wind
Diatonic harmonica numbered holes with blow/draw notation, bend levels, overblow, slide/valve where applicable, tremolo, tongue-block techniques; key- and tuning-aware lookup. Tin-whistle fingering notation requires an instrument-specific fingering map and octave/breath interpretation. Expose uncertain pitch as unresolved instead of generating misleading notes.

### P2: Symbolic and Niche
ABC (*.abc), alphaTex, CapXML/Capella, drum-machine step grids, chord diagrams, Nashville numbers and solfege where explicit musical context is available. Chord charts are not deterministic performances without a generated voicing/rhythm policy; keep them distinct from literal tabs.

## Development sequence
1. Establish golden corpus: valid, malformed, adversarial and ambiguous examples for each dialect; record expected timing/pitch and confidence.
2. Define CanonicalScore v2 and compatibility adapter for existing PerformanceScore.
3. Implement probe registry with explicit deterministic format ranking, safe bounded sniffing and ambiguity UI.
4. Implement odd ASCII, variable strings and drum notation adapters behind feature flags.
5. Add structured-format adapters after dependency/license evaluation.
6. Add harmonica/wind and symbolic adapters using dedicated profile tables.
7. Test parse -> score -> MIDI trace -> reload; compare expected samples at 44.1/48/96 kHz, loop boundaries, tempo ramps and stops.
8. Fuzz text/xml/archive entry points; differential-compare supported structured files with reference readers; test corrupt file, huge archive, zip-slip, unknown notation and missing tempo.
9. Conduct performance profiling; keep all parsing/import outside callback; expose unsupported-feature report in UI.
10. Beta documentation: matrix by format, dialect, interpretation accuracy, unsupported constructs, export compatibility and DAW routing.

## Acceptance criteria
- Every adapter has fixtures, deterministic expected-note timeline, failure-mode fixtures and provenance checks.
- No crash or uncaught exception with malformed imports; failed imports preserve active score.
- No unannounced timing assumptions for ASCII tabs without rhythm information.
- Imported notes represent correct string/course/instrument pitches and MIDI note-offs.
- Unsupported technique yields diagnostic and audible base-note fallback only when pitch/time are independently certain.
- Windows MSVC build + unit/integration tests + pluginval and manual routing test before release.

## Work coordination
Base implementation after the outstanding PR #2 CI and transport defects are resolved. Avoid conflicting edits in playback, processor, and CMake during concurrent work. Work on importer adapters and fixture corpus in isolated branches; integrate when associated tests pass.
