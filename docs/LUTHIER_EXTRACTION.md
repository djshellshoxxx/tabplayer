# Luthier extraction record

Luthier is read-only for this project.

Source repository: `djshellshoxxx/luthier`
Source ref used for extraction: `claude/luthier-consolidate`

The extracted files are the functional, reusable tablature model/import path only. The TabPlayer copies were namespace-adjusted and detached from the Luthier DSP-common include.

Copied source areas:
- `Source/Notation/PerformanceScore.*`
- `Source/Notation/AsciiTabReader.*`
- `Source/Notation/TabDocument.h`
- `Source/Notation/TabDialectLexer.*`
- `Source/Notation/TabDirectiveApplier.*`
- `Source/Notation/TabDocumentNormalizer.*`
- `Source/Notation/TabImportPipeline.*`
- `Source/Notation/TabSemanticAdapter.*`
- `Source/Notation/TabTechniqueCompiler.*`

Intentionally not copied:
- `LuthierEngine`
- `TuningEngine`
- `TabPlaybackTuningSession` implementation that mutates Luthier engine state
- Luthier UI panels
- guitar DSP
- effects
- presets
- licensing
- Luthier project/host state

TabPlayer replaces the Luthier-specific playback/tuning bridge with an instrument-neutral MIDI playback engine.
