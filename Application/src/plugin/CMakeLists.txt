cmake_minimum_required(VERSION 3.5.1)
project(SequenceFramePlugin_plugin)

if(MSVC)
    option(BUILD_SHARED_LIBS "Selects whether to build and link to shared or static libraries" ON)
endif()

if(NOT KANZI_ENGINE_BUILD)
    find_package(Kanzi REQUIRED CONFIG CMAKE_FIND_ROOT_PATH_BOTH
        HINTS "$ENV{KANZI_HOME}/Engine/lib/cmake")
endif()

include(kanzi-common)

find_package(Zlib REQUIRED)

set(sources
    lz4/lz4.c
    lz4/lz4frame.c
    lz4/lz4hc.c
    lz4/xxhash.c

    src/decompressor.cpp
    src/decompressor.h
    src/filemapping.cpp
    src/filemapping.h
    src/sequenceframeplugin.cpp
    src/sequenceframeplugin.hpp
    src/sequenceframeplugin.rc
    src/sequenceframeplugin_module.cpp
    src/sequenceframeplugin_module.hpp)

add_library(SequenceFramePlugin ${sources})
target_link_libraries(SequenceFramePlugin PUBLIC Kanzi::kzcore Kanzi::kzcoreui Kanzi::kzui Kanzidep::Zlib)

target_include_directories(SequenceFramePlugin PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/lz4 ${CMAKE_CURRENT_LIST_DIR}/zlib.lib)

if(BUILD_SHARED_LIBS AND MSVC)
    target_compile_definitions(SequenceFramePlugin PRIVATE "SEQUENCEFRAMEPLUGIN_API=__declspec(dllexport)")
    target_compile_definitions(SequenceFramePlugin PRIVATE "SEQUENCEFRAMEPLUGIN_API_EXPORT")
    target_compile_definitions(SequenceFramePlugin INTERFACE "SEQUENCEFRAMEPLUGIN_API=__declspec(dllimport)")
else()
    target_compile_definitions(SequenceFramePlugin PUBLIC "SEQUENCEFRAMEPLUGIN_API=")
endif()

# Copy plugin dll for the Kanzi-studio project.
if(BUILD_SHARED_LIBS AND MSVC)
    set(KANZI_PROJECT_PLUGIN_PATH "${PROJECT_SOURCE_DIR}/../../lib/${PLATFORM_TARGET}/${MSVC_TAG}_$<CONFIG>_DLL")
    add_custom_command(TARGET SequenceFramePlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${KANZI_PROJECT_PLUGIN_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SequenceFramePlugin>" "${KANZI_PROJECT_PLUGIN_PATH}")
endif()

install_target_to_output_directory(SequenceFramePlugin)
