# Bass tablature: research and implementation notes (2026-10-12)

## Acoustical pitch and standard layouts
Sounding bass tuning is one octave lower than guitar's lower four strings. In TabPlayer notation strings are indexed highest-pitched first. Thus four-string standard G2 D2 A1 E1 is MIDI [43,38,33,28]; five-string low-B is [43,38,33,28,23], alternate five-string high-C is [48,43,38,33,28], and six-string B E A D G C is [48,43,38,33,28,23]. Drop-D four-string [43,38,33,26], BEAD four-string [38,33,28,23]. Distinguish *sounding pitches* from transposed sheet-music notation, which is traditionally written an octave higher for bass guitar.

Sources:
- https://www.studybass.com/gear/bass-tuning/bass-guitar-tunings/
- https://lodestone.paulmederos.com/bass-tuner/
- https://www.musicstreet.co.uk/blogs/blog-post/how-to-read-bass-guitar-tabs

## Existing functionality and new support
The existing ASCII importer already recognized four-string bass-shaped G/D/A/E lines, low-B five-string named tunings, techniques in the score model (slap and pop), and a simple MIDI note timeline. The new changes prioritize bass-specific named tunings before generic guitar tunings; add explicit four-string, six-string, high-C five-string, Drop-D and BEAD bass presets; anchor unlabeled-octave six-string C/G/D/A/E/B staff labels to C3; and extend tests for notes and string pitches. Bass import currently still uses the legacy ASCII timing assumptions.

## Follow-up implementation research
- **Articulation intent vs actual MIDI rendering:** common bass symbols include x (dead-note), slap thumb, pop, hammer, pull-off, glissando, vibrato and harmonics. These should become normalized intents, then map via instrument profile into keyswitches/CC/pitch bend/velocity/duration. A generic MIDI patch cannot reproduce specialized acoustic attack from an annotation alone.
- **Muting / rests:** x is an unpitched percussive event, not fret 0; avoid assigning arbitrary sustained pitches. Ghost-note parentheses are softer but still pitched when the notation supplies a fret. Palm mute is an articulation; string damping and note-off scheduling need separate handling.
- **Bass tunings:** distinguish low-B, high-C, drop-D, BEAD, piccolo, half-step down and explicit octave tuning. Never assume a five-string configuration based only on string count if tuning labels are absent.
- **Rhythmic ambiguity:** ASCII columns can imply relative spacing but typically do not uniquely specify note durations or syncopation; strict mode should expose assumptions. A documented user-selectable subdivision and swing mode is desirable.
- **Advanced instruments:** fretless glide needs continuous pitch interpolation; octave-course 8/12-string bass requires per-course mapping rather than one pitched string per line; fan-fret/alternate temperament may require cents offsets.
- **MIDI integration:** test note off on transport stop, bass pitch range and per-string channel routing through actual bass VST instruments, including pitch bend range. Program change/keyswitch mapping must remain instrument-specific.
- **Testing:** round-trip pitch fixtures for all standard tunings, malformed ASCII fuzz, labels with octave numerals (E1), multi-digit frets, reversed-line staff order, technique symbols, and polyphony. Validate files in REAPER and FL Studio.

## Scope and verification
This branch is an incremental tuning and octave detection feature, not a complete bass-specific physical modeling or articulation engine. Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`, and `ctest --test-dir build --output-on-failure` before merge. Existing dangling-MIDI-note and real-time correctness blockers from the separate audit remain in scope for beta.
