cmake_minimum_required(VERSION 3.22)
project(Incant VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable llama.cpp integration
option(USE_LLAMA_CPP "Enable llama.cpp for LLM-based parameter generation" OFF)

# Add JUCE
add_subdirectory(libs/JUCE)

# Optionally build llama.cpp
if(USE_LLAMA_CPP)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_subdirectory(libs/llama.cpp)
endif()

# Plugin sources
set(PLUGIN_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/LLMEngine.cpp
    src/PresetManager.cpp
    src/effects/Equalizer.cpp
    src/effects/Compressor.cpp
    src/effects/Reverb.cpp
    src/effects/Distortion.cpp
    src/effects/Delay.cpp
    src/effects/Glitch.cpp
    src/effects/Overdrive.cpp
    src/effects/Chorus.cpp
    src/effects/Phaser.cpp
    src/effects/Tremolo.cpp
    src/effects/Filter.cpp
)

# Create the VST3 plugin
juce_add_plugin(Incant
    COMPANY_NAME "Incant Audio"
    BUNDLE_ID "com.incantaudio.incant"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Inct
    PLUGIN_CODE Inca
    FORMATS VST3 Standalone AU
    PRODUCT_NAME "Incant"
)

target_sources(Incant PRIVATE ${PLUGIN_SOURCES})

target_include_directories(Incant PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Incant PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link libraries
set(LINK_LIBS
    juce::juce_audio_utils
    juce::juce_dsp
)

if(USE_LLAMA_CPP)
    target_include_directories(Incant PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/llama.cpp/common
    )
    target_compile_definitions(Incant PRIVATE USE_LLAMA_CPP)
    list(APPEND LINK_LIBS llama)
endif()

target_link_libraries(Incant
    PRIVATE
        ${LINK_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Bundle the default GGUF model into plugin/app resources when present
set(INCANT_DEFAULT_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/models/Phi-4-mini-instruct.Q4_K_M.gguf")
set(INCANT_MODEL_NAME "Phi-4-mini-instruct.Q4_K_M.gguf")
if(EXISTS "${INCANT_DEFAULT_MODEL}")
    function(incant_attach_model target)
        if(TARGET ${target})
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                    -DINCANT_MODEL_FILE=${INCANT_DEFAULT_MODEL}
                    -DINCANT_MODEL_NAME=${INCANT_MODEL_NAME}
                    -DINCANT_PLUGIN_BUNDLE=$<TARGET_BUNDLE_DIR:${target}>
                    -DINCANT_PLUGIN_COPY_DIR=$<TARGET_PROPERTY:${target},JUCE_PLUGIN_COPY_DIR>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/IncantBundleModel.cmake
                VERBATIM
            )
        endif()
    endfunction()

    incant_attach_model(Incant_VST3)
    incant_attach_model(Incant_AU)
    incant_attach_model(Incant_Standalone)
endif()
