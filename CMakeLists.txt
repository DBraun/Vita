cmake_minimum_required(VERSION 3.15...3.27)
project(vita)

# Ensure Position Independent Code for shared libraries (especially important on Linux)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Allow Python executable to be specified via command line
if(DEFINED Python_EXECUTABLE)
    set(Python_ROOT_DIR "")
    set(Python_FIND_STRATEGY LOCATION)
endif()

# Find Python and nanobind
find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)

# Check if nanobind submodule is present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanobind/CMakeLists.txt")
    message(FATAL_ERROR "Nanobind submodule not found. Please run: git submodule update --init --recursive")
endif()

# Disable nanobind tests and extras to speed up configuration
set(NB_TEST OFF CACHE INTERNAL "")
set(NB_TEST_STABLE_ABI OFF CACHE INTERNAL "")
set(NB_TEST_SHARED_BUILD OFF CACHE INTERNAL "")

# Add nanobind from submodule
add_subdirectory(third_party/nanobind)

# Include directories
set(VITA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/wavetable
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/editor_components
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/editor_sections
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/look_and_feel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/wavetable
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/wavetable/editors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interface/wavetable/overlays
    ${CMAKE_CURRENT_SOURCE_DIR}/src/standalone
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/synth_engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/effects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/filters
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/framework
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/lookups
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/modulators
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/producers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/synthesis/utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Platform-specific settings
if(WIN32)
    add_definitions(-DWINDOWS=1 -D_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    add_definitions(-DMAC=1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else()
    add_definitions(-DLINUX=1)
    find_package(Threads REQUIRED)
endif()

# Common definitions
add_definitions(
    -DHEADLESS=1
    -DNO_AUTH=1
    -DJUCE_STANDALONE_APPLICATION=0
    -DJUCE_USE_CURL=0
    -DJUCE_WEB_BROWSER=0
    -DJUCE_USE_CAMERA=0
    -DJUCE_GUI_BASICS=0
    -DJUCE_GRAPHICS=0
    -DJUCE_EVENTS=1
    -DJUCE_AUDIO_BASICS=1
    -DJUCE_AUDIO_FORMATS=1
    -DJUCE_AUDIO_PROCESSORS=0
    -DJUCE_AUDIO_DEVICES=0
    -DJUCE_AUDIO_UTILS=0
    -DJUCE_CORE=1
    -DJUCE_DATA_STRUCTURES=1
    -DJUCE_DSP=1
    -DJUCE_CRYPTOGRAPHY=0
    -DJUCE_OPENGL=0
    -DJUCE_VIDEO=0
    -DJUCE_PRODUCT_ANALYTICS=0
)

# JUCE module sources
set(JUCE_SOURCES
    headless/JuceLibraryCode/include_juce_audio_basics.cpp
    headless/JuceLibraryCode/include_juce_audio_formats.cpp
    headless/JuceLibraryCode/include_juce_core.cpp
    headless/JuceLibraryCode/include_juce_data_structures.cpp
    headless/JuceLibraryCode/include_juce_dsp.cpp
    headless/JuceLibraryCode/include_juce_events.cpp
)

# On macOS, JUCE sources need to be compiled as Objective-C++
if(APPLE)
    set_source_files_properties(${JUCE_SOURCES} PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

# Create the Python module
nanobind_add_module(
    _vita
    src/headless/bindings.cpp
    src/unity_build/common.cpp
    src/unity_build/synthesis.cpp
    ${JUCE_SOURCES}
)

# Set C++ standard
target_compile_features(_vita PRIVATE cxx_std_17)

# Include directories
target_include_directories(_vita PRIVATE
    ${VITA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/headless/JuceLibraryCode
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules
)

# Platform-specific linking
if(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(COREMIDI_LIBRARY CoreMIDI)
    find_library(ACCELERATE_LIBRARY Accelerate)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    
    target_link_libraries(_vita PRIVATE
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREAUDIO_LIBRARY}
        ${COREMIDI_LIBRARY}
        ${ACCELERATE_LIBRARY}
        ${AUDIOTOOLBOX_LIBRARY}
        ${AUDIOUNIT_LIBRARY}
        ${QUARTZCORE_LIBRARY}
    )
elseif(WIN32)
    target_link_libraries(_vita PRIVATE
        winmm
        wininet
        ws2_32
        version
        shlwapi
    )
elseif(UNIX)
    target_link_libraries(_vita PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
        rt
    )
    
    # Find ALSA on Linux
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(ALSA alsa)
        if(ALSA_FOUND)
            target_include_directories(_vita PRIVATE ${ALSA_INCLUDE_DIRS})
            target_link_libraries(_vita PRIVATE ${ALSA_LIBRARIES})
        endif()
    endif()
endif()

# Installation rules for scikit-build-core
install(TARGETS _vita LIBRARY DESTINATION vita COMPONENT python_modules)

# Install Python package files
install(FILES 
    vita/__init__.py 
    vita/version.py
    vita/LICENSE
    DESTINATION vita 
    COMPONENT python_modules
)

# Install constants directory
install(DIRECTORY vita/constants 
    DESTINATION vita 
    COMPONENT python_modules
)