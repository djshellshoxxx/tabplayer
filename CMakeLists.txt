cmake_minimum_required(VERSION 3.22)
if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(TabPlayer VERSION 0.1.0 LANGUAGES C CXX)

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
    src/playback/TechniqueCompiler.cpp
    src/profile/InstrumentProfile.cpp
    src/project/ProjectFile.cpp
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
    VST3_AUTO_MANIFEST FALSE
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
    src/profile
    src/project
    src/plugin
)

target_compile_definitions(TabPlayer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
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
    # One executable per tests/*Tests.cpp so a failing suite can be rebuilt/run alone.
    file(GLOB TABPLAYER_TEST_SOURCES CONFIGURE_DEPENDS tests/*Tests.cpp)
    foreach (test_src ${TABPLAYER_TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src} ${TABPLAYER_CORE})
        target_include_directories(${test_name} PRIVATE src src/core src/import src/playback src/profile src/project)
        target_link_libraries(${test_name} PRIVATE juce::juce_audio_basics juce::juce_core juce::juce_events)
        target_compile_definitions(${test_name} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
