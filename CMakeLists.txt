cmake_minimum_required(VERSION 3.22)
project(CrayLdrUrbanInstruments VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /MP /permissive- /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(CRAY_BUILD_TESTS "Build and run CRAY LDR JUCE unit tests" ON)
option(CRAY_BUILD_STANDALONE "Build Standalone application alongside VST3" ON)
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE 8.0.6 checkout")

if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(STATUS "Using local JUCE from: ${JUCE_DIR}")
  add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce_build")
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# This checkout stores implementation files at the repository root.
# Keep explicit source lists so test runners never get linked into the plugin.
set(CRAY_CORE_SOURCES
  ParameterIDs.cpp
  PolyBlepOscillator.cpp
  UnisonOscillatorBank.cpp
  AhdsrEnvelope.cpp
  ZdfSvfFilter.cpp
  LfoEngine.cpp
  ModMatrix.cpp
  ParametricEq.cpp
  SaturationDistortion.cpp
  StereoChorus.cpp
  StereoDelay.cpp
  AlgorithmicReverb.cpp
  EffectsChain.cpp
  SynthEngine.cpp
  SampleData.cpp
  SampleEngine.cpp
  VoiceManager.cpp
  AudioEngine.cpp
  SampleDecoder.cpp
  LibraryManager.cpp
  SampleEditorModel.cpp
  InstrumentDefinition.cpp
  InstrumentBuilder.cpp
  PresetManager.cpp
)

set(CRAY_PLUGIN_SOURCES
  PluginProcessor.cpp
  PluginEditor.cpp
  CrayLookAndFeel.cpp
  RotaryKnob.cpp
  LevelMeter.cpp
  EnvelopeDisplay.cpp
  WaveformView.cpp
  KeyZoneMapView.cpp
  VirtualKeyboardBar.cpp
  HeaderBar.cpp
  BrowserSidebar.cpp
  MainView.cpp
  EditView.cpp
  SampleEditorView.cpp
  InstrumentBuilderView.cpp
  EffectsView.cpp
)

foreach(source IN LISTS CRAY_CORE_SOURCES CRAY_PLUGIN_SOURCES)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    message(FATAL_ERROR "Required source file is missing: ${source}")
  endif()
endforeach()

# Preserve logical include paths from the original modular layout while
# resolving each header to its actual root-level file.
file(GLOB CRAY_HEADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
set(CRAY_COMPAT_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated_compat_includes")
file(MAKE_DIRECTORY "${CRAY_COMPAT_INCLUDE_DIR}")
foreach(source_file IN LISTS CRAY_CORE_SOURCES CRAY_PLUGIN_SOURCES CRAY_HEADER_FILES)
  if(NOT IS_ABSOLUTE "${source_file}")
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
  endif()
  if(EXISTS "${source_file}")
    file(STRINGS "${source_file}" include_lines
      REGEX "^[ \t]*#[ \t]*include[ \t]*\"[^\"]+\"")
    foreach(include_line IN LISTS include_lines)
      string(REGEX MATCH "\"([^\"]+)\"" include_match "${include_line}")
      if(NOT include_match)
        continue()
      endif()
      set(include_path "${CMAKE_MATCH_1}")
      get_filename_component(include_leaf "${include_path}" NAME)
      set(real_header "${CMAKE_CURRENT_SOURCE_DIR}/${include_leaf}")
      if(EXISTS "${real_header}" AND NOT include_path STREQUAL include_leaf)
        set(forward_header "${CRAY_COMPAT_INCLUDE_DIR}/${include_path}")
        get_filename_component(forward_dir "${forward_header}" DIRECTORY)
        file(MAKE_DIRECTORY "${forward_dir}")
        file(WRITE "${forward_header}" "#pragma once\n#include \"${real_header}\"\n")
      endif()
    endforeach()
  endif()
endforeach()

add_library(CrayLdrCore STATIC ${CRAY_CORE_SOURCES})
target_include_directories(CrayLdrCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CRAY_COMPAT_INCLUDE_DIR}")
target_link_libraries(CrayLdrCore PUBLIC
  juce::juce_audio_basics
  juce::juce_audio_formats
  juce::juce_audio_processors
  juce::juce_core
  juce::juce_data_structures
  juce::juce_dsp
  juce::juce_events)
target_compile_definitions(CrayLdrCore PUBLIC
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_FLAC=1
  JUCE_USE_OGGVORBIS=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

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
  VST3_CATEGORIES Instrument Synth Sampler)

juce_generate_juce_header(CrayLdrUrbanInstruments)
target_sources(CrayLdrUrbanInstruments PRIVATE ${CRAY_PLUGIN_SOURCES})
target_include_directories(CrayLdrUrbanInstruments PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CRAY_COMPAT_INCLUDE_DIR}")
target_link_libraries(CrayLdrUrbanInstruments PRIVATE
  CrayLdrCore
  juce::juce_audio_utils
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
target_compile_definitions(CrayLdrUrbanInstruments PUBLIC
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_USE_FLAC=1
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_VERSION>")

# Factory resources are optional in the current repository snapshot.
foreach(resource_dir FactoryPresets FactorySamples)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/${resource_dir}")
    add_custom_command(TARGET CrayLdrUrbanInstruments POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/${resource_dir}"
        "$<TARGET_FILE_DIR:CrayLdrUrbanInstruments>/Resources/${resource_dir}"
      VERBATIM)
  endif()
endforeach()

if(CRAY_BUILD_TESTS)
  set(CRAY_TEST_SOURCES
    TestMain.cpp
    TestOscillatorsAndFilters.cpp
    TestSampleEngineAndResampling.cpp
    TestVoiceManagerAndPolyphony.cpp
    TestEffectsAndModMatrix.cpp
    TestLibraryAndInstrumentBuilder.cpp
    TestStateAndPresetSerialization.cpp)
  foreach(source IN LISTS CRAY_TEST_SOURCES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
      message(FATAL_ERROR "CRAY_BUILD_TESTS=ON but test source is missing: ${source}")
    endif()
  endforeach()
  add_executable(CrayLdrTests ${CRAY_TEST_SOURCES})
  juce_generate_juce_header(CrayLdrTests)
  target_include_directories(CrayLdrTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CRAY_COMPAT_INCLUDE_DIR}")
  target_link_libraries(CrayLdrTests PRIVATE
    CrayLdrCore
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
  target_compile_definitions(CrayLdrTests PRIVATE
    CRAY_TEST_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
  enable_testing()
  add_test(NAME CrayLdrUnitTests COMMAND CrayLdrTests)
endif()

message(STATUS "CRAY LDR: root-layout source map configured")
message(STATUS "CRAY LDR: unit tests enabled = ${CRAY_BUILD_TESTS}")
