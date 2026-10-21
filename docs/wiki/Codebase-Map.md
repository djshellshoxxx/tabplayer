# Codebase Map

| I want to... | Look in |
|---|---|
| Tab document model and shared types | `src/core/TabDocument.h`, `src/core/TabPlayerTypes.h` |
| Scoring | `src/core/PerformanceScore.*` |
| ASCII tab import | `src/import/AsciiTabReader.*`, `TabDialectLexer.*`, `TabDirectiveApplier.*` |
| Normalisation and semantics | `src/import/TabDocumentNormalizer.*`, `TabSemanticAdapter.*` |
| Technique compilation and the import pipeline | `src/import/TabTechniqueCompiler.*`, `TabImportPipeline.*` |
| MIDI playback timeline | `src/playback/TabPlaybackEngine.*` |
| Plugin processor and editor | `src/plugin/PluginProcessor.*`, `PluginEditor.*` |
| Tests | `tests/PlaybackEngineTests.cpp` |
| Specs | `docs/ENGINEERING_SPEC.md`, `docs/LUTHIER_EXTRACTION.md` |
| Build and CI | `CMakeLists.txt`, `.github/workflows/ci.yml` |
