# TabPlayer

TabPlayer is a standalone tablature playback engine and MIDI-control plug-in.

The project begins by extracting the functional ASCII-tab import/playback work from Luthier into an independent codebase. Luthier remains read-only: no TabPlayer work is written back to the Luthier repository.

See `docs/ENGINEERING_SPEC.md` for the product and engineering specification.

Current bootstrap: extracted tab import engine, MIDI playback timeline, VST3/Standalone shell, and visual tab view.
