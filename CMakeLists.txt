# =============================================================================
#  METAL//MACHINE - CMake principal
#  Gera: VST3 (principal) + Standalone (app para testes sem DAW)
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(METAL_MACHINE VERSION 0.17.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime estatico no Windows: o usuario NAO precisa instalar "VC++ Redistributable"
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# -----------------------------------------------------------------------------
# JUCE: se existir a pasta ./JUCE usa ela; senao baixa automaticamente do GitHub
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# Plugin
# -----------------------------------------------------------------------------
juce_add_plugin(MetalMachine
    COMPANY_NAME                "ATDT Audio"
    PRODUCT_NAME                "ATDT SYNTH MACHINE"
    BUNDLE_ID                   com.atdtaudio.metalmachine
    VERSION                     0.17.0
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Mtlm
    PLUGIN_CODE                 Mmc1
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Instrument Synth)

target_sources(MetalMachine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters/Parameters.cpp
    Source/Synth/SynthVoice.cpp
    Source/UI/MetalLookAndFeel.cpp
    Source/UI/Displays.cpp
    Source/UI/SynthPage.cpp
    Source/UI/MainView.cpp
    Source/UI/ModPage.cpp
    Source/UI/PresetBrowser.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Sequencer/StepSequencer.cpp
    Source/Sequencer/PatternLibrary.cpp
    Source/UI/SeqPage.cpp
    Source/UI/FxPage.cpp
    Source/UI/DrumsPage.cpp
    Source/UI/SongPage.cpp
    Source/UI/RefLabPage.cpp
    Source/UI/SideBrowser.cpp
    Source/UI/ScratchPage.cpp
    Source/UI/ScratchPad.cpp)

target_include_directories(MetalMachine PRIVATE Source)

target_compile_definitions(MetalMachine PUBLIC
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MetalMachine
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
# Testes automaticos (opcional): cmake -B build -DMM_BUILD_TESTS=ON
# -----------------------------------------------------------------------------
option(MM_BUILD_TESTS "Compila o host de teste PluginTest" OFF)
if(MM_BUILD_TESTS)
    juce_add_console_app(PluginTest PRODUCT_NAME "PluginTest")
    target_sources(PluginTest PRIVATE Tests/PluginTest.cpp)
    target_include_directories(PluginTest PRIVATE Source)
    target_compile_definitions(PluginTest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PluginTest PRIVATE
        juce::juce_audio_processors juce::juce_audio_utils juce::juce_dsp juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
