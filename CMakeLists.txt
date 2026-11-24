cmake_minimum_required(VERSION 3.22)

project(CrayLdrUrbanInstruments VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /MP /permissive- /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(CRAY_BUILD_TESTS "Build CRAY LDR unit test suite" ON)
option(CRAY_BUILD_STANDALONE "Build Standalone application alongside VST3" ON)

# JUCE 8.0.6 resolution: use local JUCE_DIR if supplied, otherwise FetchContent
set(JUCE_DIR "" CACHE PATH "Optional path to local JUCE 8.0.6 checkout")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE from: ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce_build")
else()
    include(FetchContent)
    message(STATUS "Fetching JUCE 8.0.6 via FetchContent...")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(CRAY_PLUGIN_FORMATS VST3)
if(CRAY_BUILD_STANDALONE)
    list(APPEND CRAY_PLUGIN_FORMATS Standalone)
endif()

juce_add_plugin(CrayLdrUrbanInstruments
    COMPANY_NAME "CRAY LDR"
    COMPANY_WEBSITE "https://crayldr.audio"
    COMPANY_EMAIL "support@crayldr.audio"
    BUNDLE_ID "com.crayldr.urbaninstruments"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE CrLd
    PLUGIN_CODE CrUr
    FORMATS ${CRAY_PLUGIN_FORMATS}
    PRODUCT_NAME "CRAY LDR URBAN INSTRUMENTS"
    VST3_CATEGORIES "Instrument|Synth|Sampler"
)

juce_generate_juce_header(CrayLdrUrbanInstruments)

# Binary resources (Graphics + Factory manifest if embedded)
file(GLOB_RECURSE CRAY_GRAPHIC_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Graphics/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Graphics/*.png"
)
if(CRAY_GRAPHIC_ASSETS)
    juce_add_binary_data(CrayLdrBinaryData SOURCES ${CRAY_GRAPHIC_ASSETS})
else()
    # Placeholder empty binary data target avoided; Resources/Graphics will always contain brand SVGs
    message(WARNING "No graphic assets found in Resources/Graphics")
endif()

set(CRAY_CORE_SOURCES
    Source/Common/ParameterIDs.cpp
    Source/DSP/Oscillators/PolyBlepOscillator.cpp
    Source/DSP/Oscillators/UnisonOscillatorBank.cpp
    Source/DSP/Envelopes/AhdsrEnvelope.cpp
    Source/DSP/Filters/ZdfSvfFilter.cpp
    Source/DSP/Modulation/LfoEngine.cpp
    Source/DSP/Modulation/ModMatrix.cpp
    Source/DSP/Effects/ParametricEq.cpp
    Source/DSP/Effects/SaturationDistortion.cpp
    Source/DSP/Effects/StereoChorus.cpp
    Source/DSP/Effects/StereoDelay.cpp
    Source/DSP/Effects/AlgorithmicReverb.cpp
    Source/DSP/Effects/EffectsChain.cpp
    Source/Engine/SynthEngine.cpp
    Source/Engine/SampleData.cpp
    Source/Engine/SampleEngine.cpp
    Source/Engine/VoiceManager.cpp
    Source/Engine/AudioEngine.cpp
    Source/Library/SampleDecoder.cpp
    Source/Library/LibraryManager.cpp
    Source/Library/SampleEditorModel.cpp
    Source/Library/InstrumentDefinition.cpp
    Source/Library/InstrumentBuilder.cpp
    Source/Library/PresetManager.cpp
)

set(CRAY_PLUGIN_SOURCES
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/UI/Theme/CrayLookAndFeel.cpp
    Source/UI/Components/RotaryKnob.cpp
    Source/UI/Components/LevelMeter.cpp
    Source/UI/Components/EnvelopeDisplay.cpp
    Source/UI/Components/WaveformView.cpp
    Source/UI/Components/KeyZoneMapView.cpp
    Source/UI/Components/VirtualKeyboardBar.cpp
    Source/UI/Components/HeaderBar.cpp
    Source/UI/Components/BrowserSidebar.cpp
    Source/UI/Views/MainView.cpp
    Source/UI/Views/EditView.cpp
    Source/UI/Views/SampleEditorView.cpp
    Source/UI/Views/InstrumentBuilderView.cpp
    Source/UI/Views/EffectsView.cpp
)

# Static core library shared between plugin and unit tests
add_library(CrayLdrCore STATIC ${CRAY_CORE_SOURCES})
target_include_directories(CrayLdrCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)
target_link_libraries(CrayLdrCore PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
)
target_compile_definitions(CrayLdrCore PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_sources(CrayLdrUrbanInstruments PRIVATE ${CRAY_PLUGIN_SOURCES})

target_include_directories(CrayLdrUrbanInstruments PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_link_libraries(CrayLdrUrbanInstruments PRIVATE
    CrayLdrCore
    $<$<TARGET_EXISTS:CrayLdrBinaryData>:CrayLdrBinaryData>
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_definitions(CrayLdrUrbanInstruments PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_VERSION>"
)

# Copy Factory Library alongside build outputs for local testing
add_custom_command(TARGET CrayLdrUrbanInstruments POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactoryPresets"
        "$<TARGET_FILE_DIR:CrayLdrUrbanInstruments>/Resources/FactoryPresets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactorySamples"
        "$<TARGET_FILE_DIR:CrayLdrUrbanInstruments>/Resources/FactorySamples"
    COMMENT "Staging Factory Library resources..."
)

if(CRAY_BUILD_TESTS)
    enable_testing()
    add_executable(CrayLdrTests
        Tests/TestMain.cpp
        Tests/TestOscillatorsAndFilters.cpp
        Tests/TestSampleEngineAndResampling.cpp
        Tests/TestVoiceManagerAndPolyphony.cpp
        Tests/TestEffectsAndModMatrix.cpp
        Tests/TestLibraryAndInstrumentBuilder.cpp
        Tests/TestStateAndPresetSerialization.cpp
    )
    target_include_directories(CrayLdrTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests"
    )
    target_link_libraries(CrayLdrTests PRIVATE
        CrayLdrCore
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    target_compile_definitions(CrayLdrTests PRIVATE
        CRAY_TEST_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources"
    )
    add_test(NAME CrayLdrUnitTests COMMAND CrayLdrTests)
endif()
