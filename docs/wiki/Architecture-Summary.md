# Architecture Summary

Pipeline (README): tab file → parser → normalised score → performance compiler → transport → MIDI output.

- **Import** (`src/import/`): an ASCII tab reader, a dialect lexer, a directive applier, a document normaliser, a semantic adapter, a technique compiler and an import pipeline.
- **Core types** (`src/core/`): the tab document, shared types and the performance score with its scoring code.
- **Playback** (`src/playback/`): the playback engine that turns the score into a timed MIDI sequence.
- **Plugin shell** (`src/plugin/`): the JUCE processor and editor.
- **Reuse.** The tab engine was extracted from Luthier, but TabPlayer has its own namespace and types, and the Luthier repository is not changed (README, `docs/LUTHIER_EXTRACTION.md`).
- **Not yet confirmed:** hosting an instrument inside TabPlayer and wider tablature-format support are design goals only.
