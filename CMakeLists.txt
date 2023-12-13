cmake_minimum_required(VERSION 3.12)
project(ProTerGen LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRCS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HDRS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB_RECURSE SHDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl)
file(GLOB_RECURSE SHIS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsli)

add_subdirectory("ext/DirectX-Headers")
add_subdirectory("ext/DirectXMath")
add_subdirectory("ext/DirectXTK12")

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS ${PROJECT_SOURCE_DIR}/.git)
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/ext/DirectXMath/CMakeLists.txt)
    message(FATAL_ERROR "The DirectXMath submodules was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/ext/DirectX-Headers/CMakeLists.txt)
    message(FATAL_ERROR "The DirectX-Headers submodules was not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

set(ICON_FILES
${CMAKE_CURRENT_SOURCE_DIR}/icon/icon.rc
${CMAKE_CURRENT_SOURCE_DIR}/icon/resource.h
${CMAKE_CURRENT_SOURCE_DIR}/icon/logo.ico
)
source_group("Icon" FILES ${ICON_FILES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t")

add_definitions(-D_UNICODE -DUNICODE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_SOURCE_DIR}/bin>)

include_directories("ext/imgui" "ext/imgui/backends")
add_library(
    imgui
    "ext/imgui/imgui.cpp"
    "ext/imgui/imgui_demo.cpp"
    "ext/imgui/imgui_draw.cpp"
    "ext/imgui/imgui_widgets.cpp"
    "ext/imgui/imgui_tables.cpp"
    "ext/imgui/backends/imgui_impl_win32.cpp"
    "ext/imgui/backends/imgui_impl_dx12.cpp"
    "ext/imgui/imgui.h"
    "ext/imgui/backends/imgui_impl_win32.h"
    "ext/imgui/backends/imgui_impl_dx12.h"
)

add_executable(${PROJECT_NAME}
WIN32
${HDRS}
${SRCS}
${SHDS}
${SHIS}
${ICON_FILES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    "ext/DirectX-Headers/include/directx"
    "ext/DirectXMath/include"
    "ext/DirectXTK12/Inc"
    "ext/imgui"
    "icon"
    "include"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    d3d12.lib
    dxgi.lib
    dxguid.lib
    D3DCompiler.lib
    Shlwapi.lib
    DirectXMath
    DirectXTK12
    imgui
)

add_custom_target(shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Shaders
    COMMENT "Copying resources into binary directory"
)

add_dependencies(${PROJECT_NAME} shaders)

set_directory_properties(PROPERTIES
    VS_STARTUP_PROJECT ${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    LINK_FLAGS /SUBSYSTEM:CONSOLE
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_property(SOURCE ${SHDS} ${SHIS} PROPERTY VS_SETTINGS "ExcludedFromBuild=true")
