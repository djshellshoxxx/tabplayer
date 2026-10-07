cmake_minimum_required(VERSION 3.22)
project(TabPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(TABPLAYER_CORE
    src/core/PerformanceScore.cpp
    src/import/AsciiTabReader.cpp
    src/import/TabDialectLexer.cpp
    src/import/TabDirectiveApplier.cpp
    src/import/TabDocumentNormalizer.cpp
    src/import/TabImportPipeline.cpp
    src/import/TabSemanticAdapter.cpp
    src/import/TabTechniqueCompiler.cpp
    src/playback/TabPlaybackEngine.cpp
)

juce_add_plugin(TabPlayer
    COMPANY_NAME "Circuit Drift Labs"
    BUNDLE_ID "com.circuitdriftlabs.tabplayer"
    PLUGIN_MANUFACTURER_CODE CDrf
    PLUGIN_CODE TbPl
    FORMATS VST3 Standalone
    PRODUCT_NAME "TabPlayer"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(TabPlayer PRIVATE
    ${TABPLAYER_CORE}
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
)

target_include_directories(TabPlayer PRIVATE
    src
    src/core
    src/import
    src/playback
    src/plugin
)

target_compile_definitions(TabPlayer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(TabPlayer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
)

option(TABPLAYER_BUILD_TESTS "Build TabPlayer tests" ON)
if (TABPLAYER_BUILD_TESTS)
    enable_testing()
    add_executable(TabPlayerTests tests/PlaybackEngineTests.cpp ${TABPLAYER_CORE})
    target_include_directories(TabPlayerTests PRIVATE src src/core src/import src/playback)
    target_link_libraries(TabPlayerTests PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_events
    )
    add_test(NAME TabPlayerTests COMMAND TabPlayerTests)
endif()
