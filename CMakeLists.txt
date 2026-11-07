cmake_minimum_required(VERSION 3.22)
project(HeavyAmpSim VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
option(HEAVY_BUILD_PLUGIN "Build the JUCE VST3 and standalone app" ON)
option(HEAVY_SANITIZERS "Instrument core tests with address/undefined sanitizers" OFF)
option(HEAVY_BUILD_UI_TOOLS "Build the actual-editor PNG capture tool" OFF)
add_library(heavy_dsp STATIC src/dsp/Engine.cpp src/dsp/Circuit.cpp)
target_include_directories(heavy_dsp PUBLIC src)
if(MSVC)
  target_compile_options(heavy_dsp PRIVATE /W4)
else()
  target_compile_options(heavy_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(HEAVY_SANITIZERS AND NOT MSVC)
  target_compile_options(heavy_dsp PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(heavy_dsp PUBLIC -fsanitize=address,undefined)
endif()
enable_testing()
add_executable(heavy_dsp_tests tests/EngineTests.cpp)
target_link_libraries(heavy_dsp_tests PRIVATE heavy_dsp)
add_test(NAME dsp COMMAND heavy_dsp_tests)
add_executable(heavy_circuit_tests tests/CircuitTests.cpp)
target_link_libraries(heavy_circuit_tests PRIVATE heavy_dsp)
target_compile_definitions(heavy_circuit_tests PRIVATE
  HEAVY_CIRCUIT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/model_t_tone_ngspice.csv"
  HEAVY_VALVE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/model_t_valve_ngspice.csv")
add_test(NAME circuit COMMAND heavy_circuit_tests)
if(HEAVY_BUILD_PLUGIN)
  if(DEFINED JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209)
    FetchContent_MakeAvailable(JUCE)
  endif()
  juce_add_plugin(HeavyAmpSim
    COMPANY_NAME VerdilacSound
    BUNDLE_ID com.verdilacsound.heavyampsim
    PLUGIN_MANUFACTURER_CODE Vdls
    PLUGIN_CODE Hvas
    FORMATS VST3 Standalone
    PRODUCT_NAME "HeavyAmpSim"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion)
  target_sources(HeavyAmpSim PRIVATE src/plugin/Processor.cpp src/plugin/Editor.cpp)
  target_compile_definitions(HeavyAmpSim PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(HeavyAmpSim PRIVATE heavy_dsp juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  add_executable(heavy_plugin_tests tests/PluginTests.cpp)
  # Reuse the plugin's actual JUCE objects and configuration, rather than
  # compiling a second copy of JUCE with conflicting standalone definitions.
  target_include_directories(heavy_plugin_tests PRIVATE $<TARGET_PROPERTY:HeavyAmpSim,INCLUDE_DIRECTORIES>)
  target_compile_definitions(heavy_plugin_tests PRIVATE $<TARGET_PROPERTY:HeavyAmpSim,COMPILE_DEFINITIONS>)
  target_link_libraries(heavy_plugin_tests PRIVATE HeavyAmpSim heavy_dsp)
  add_test(NAME plugin COMMAND heavy_plugin_tests)
  if(HEAVY_BUILD_UI_TOOLS)
    add_executable(heavy_ui_snapshot tools/UiSnapshot.cpp)
    target_include_directories(heavy_ui_snapshot PRIVATE $<TARGET_PROPERTY:HeavyAmpSim,INCLUDE_DIRECTORIES>)
    target_compile_definitions(heavy_ui_snapshot PRIVATE $<TARGET_PROPERTY:HeavyAmpSim,COMPILE_DEFINITIONS>)
    target_link_libraries(heavy_ui_snapshot PRIVATE HeavyAmpSim heavy_dsp)
  endif()
endif()
