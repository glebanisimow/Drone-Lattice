# Drone Lattice (standalone + VST3). Faust code is generated at build time from
# ../engines/*.dsp.
cmake_minimum_required(VERSION 3.22)
project(DroneLattice VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Release builds keep PDBs next to the binaries so a crash backtrace names
# functions and lines.
if(MSVC)
    add_compile_options("$<$<CONFIG:Release>:/Zi;/FS>")
    add_link_options("$<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF>")
endif()

set(TOOLS "$ENV{LOCALAPPDATA}/drone-tools")
set(JUCE_DIR "${TOOLS}/JUCE" CACHE PATH "JUCE source tree")
find_program(FAUST faust HINTS "${TOOLS}/faust/bin" REQUIRED)
get_filename_component(FAUST_BIN "${FAUST}" DIRECTORY)
get_filename_component(FAUST_INCLUDE "${FAUST_BIN}/../include" ABSOLUTE)

add_subdirectory(${JUCE_DIR} JUCE)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY ${GEN})

set(GEN_HEADERS)
foreach(pair lattice_swarm:LatticeSwarm res_mesh:ResMesh mod_bus:ModBus)
    string(REPLACE ":" ";" p ${pair})
    list(GET p 0 name)
    list(GET p 1 cls)
    add_custom_command(
        OUTPUT ${GEN}/${name}.h
        COMMAND ${FAUST} -I ${ROOT}/lib -ftz 2 -lang cpp -cn ${cls}
                ${ROOT}/engines/${name}.dsp -o ${GEN}/${name}.h
        DEPENDS ${ROOT}/engines/${name}.dsp ${ROOT}/lib/contract.lib
        COMMENT "faust ${name}.dsp -> ${cls}")
    list(APPEND GEN_HEADERS ${GEN}/${name}.h)
endforeach()

# One target owns the generated headers so the Visual Studio generator does not
# run the same custom command twice.
add_custom_target(faust_gen DEPENDS ${GEN_HEADERS})

set(GEN_SOURCES src/EngineLatticeSwarm.cpp src/EngineResMesh.cpp src/EngineModBus.cpp)
# Generated Faust code: IEEE floats (the contract polynomials and wraps assume
# them).
set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/W0;/O2")

function(drone_app target)
    add_dependencies(${target} faust_gen)
    target_include_directories(${target} PRIVATE src ${GEN} ${FAUST_INCLUDE})
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${target},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${target},JUCE_VERSION>")
    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

juce_add_plugin(DroneLattice
    PRODUCT_NAME "Drone Lattice"
    COMPANY_NAME "drone-lattice"
    BUNDLE_ID com.dronelattice.app
    PLUGIN_MANUFACTURER_CODE Drnl
    PLUGIN_CODE Dlt1
    FORMATS Standalone VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    # The manifest step loads the fresh DLL at build time; Smart App Control
    # may block that unsigned load, so it is skipped (hosts scan without it).
    VST3_AUTO_MANIFEST FALSE
    ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png)

set(DRONE_SOURCES
    src/EngineLatticeSwarm.cpp
    src/EngineResMesh.cpp
    src/EngineModBus.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/LatticeView.cpp)

target_sources(DroneLattice PRIVATE ${DRONE_SOURCES} src/Widgets.cpp src/entry/DroneEntry.cpp)
drone_app(DroneLattice)
