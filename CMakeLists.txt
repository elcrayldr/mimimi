cmake_minimum_required(VERSION 3.22)
project(CrayLdrUrbanInstruments VERSION 1.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/W4 /MP /permissive- /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
option(CRAY_BUILD_TESTS "Build CRAY LDR unit tests (requires Tests/ sources)" OFF)
option(CRAY_BUILD_STANDALONE "Build Standalone application alongside VST3" ON)
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(STATUS "Using local JUCE from: ${JUCE_DIR}")
  add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce_build")
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# The checked-in sources currently live in the repository root.
file(GLOB CRAY_CPP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB CRAY_HEADER_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
if(NOT CRAY_CPP_SOURCES)
  message(FATAL_ERROR "No root-level C++ source files were found.")
endif()

# Generate forwarding headers for logical include paths such as
# Engine/AudioEngine.h -> ./AudioEngine.h, without altering source files.
set(CRAY_COMPAT_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated_compat_includes")
file(MAKE_DIRECTORY "${CRAY_COMPAT_INCLUDE_DIR}")
foreach(source_file IN LISTS CRAY_CPP_SOURCES CRAY_HEADER_FILES)
  file(STRINGS "${source_file}" include_lines REGEX "^[ \t]*#[ \t]*include[ \t]*\"[^\"]+\"")
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
endforeach()

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
  VST3_CATEGORIES Instrument Synth Sampler
)
juce_generate_juce_header(CrayLdrUrbanInstruments)
target_sources(CrayLdrUrbanInstruments PRIVATE ${CRAY_CPP_SOURCES} ${CRAY_HEADER_FILES})
target_include_directories(CrayLdrUrbanInstruments PRIVATE "${CRAY_COMPAT_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CrayLdrUrbanInstruments PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_audio_formats
  juce::juce_audio_basics juce::juce_dsp juce::juce_gui_basics juce::juce_gui_extra
  juce::juce_data_structures juce::juce_events juce::juce_core
  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
target_compile_definitions(CrayLdrUrbanInstruments PUBLIC
  JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CrayLdrUrbanInstruments,JUCE_VERSION>"
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactoryPresets")
  add_custom_command(TARGET CrayLdrUrbanInstruments POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactoryPresets"
      "$<TARGET_FILE_DIR:CrayLdrUrbanInstruments>/Resources/FactoryPresets" VERBATIM)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactorySamples")
  add_custom_command(TARGET CrayLdrUrbanInstruments POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/Resources/FactorySamples"
      "$<TARGET_FILE_DIR:CrayLdrUrbanInstruments>/Resources/FactorySamples" VERBATIM)
endif()
if(CRAY_BUILD_TESTS)
  message(WARNING "Unit tests are not included in this repository snapshot; skipping test target.")
endif()
message(STATUS "CRAY LDR source layout: root-level C++ files")
message(STATUS "CRAY LDR translation units: ${CRAY_CPP_SOURCES}")
