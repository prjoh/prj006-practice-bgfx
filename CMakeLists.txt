cmake_minimum_required(VERSION 3.25)

project(3d-graphics-starter LANGUAGES CXX)

# Set the source directory
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/Source")

# Check the platform and set the project directories accordingly
if (CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    set(BINARY_DIR "${CMAKE_SOURCE_DIR}/Bin/x64")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BINARY_DIR "${CMAKE_SOURCE_DIR}/Bin/linux")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(BINARY_DIR "${CMAKE_SOURCE_DIR}/Bin/osx")
# TODO: Quit for unknown platform
endif()

add_compile_options("-DBGFX_BUILD_EXAMPLES=OFF")

# Include the CMakeLists.txt for dependencies
add_subdirectory(${SOURCE_DIR}/ThirdParty/bgfx.cmake)
add_subdirectory(${SOURCE_DIR}/ThirdParty/imgui)
add_subdirectory(${SOURCE_DIR}/ThirdParty/SDL2)

# Specify the source files
include_directories(${SOURCE_DIR})
add_executable(${PROJECT_NAME}
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/Loading.cpp
    ${SOURCE_DIR}/Loading.h
    ${SOURCE_DIR}/Input.cpp
    ${SOURCE_DIR}/Input.h
    ${SOURCE_DIR}/Transform.cpp
    ${SOURCE_DIR}/Transform.h
    ${SOURCE_DIR}/Camera.cpp
    ${SOURCE_DIR}/Camera.h
    ${SOURCE_DIR}/Object3D.cpp
    ${SOURCE_DIR}/Object3D.h
    ${SOURCE_DIR}/Mesh.cpp
    ${SOURCE_DIR}/Mesh.h
    ${SOURCE_DIR}/MaterialBase.cpp
    ${SOURCE_DIR}/MaterialBase.h
    ${SOURCE_DIR}/Materials.cpp
    ${SOURCE_DIR}/Materials.h
    ${SOURCE_DIR}/GeometryBase.cpp
    ${SOURCE_DIR}/GeometryBase.h
    ${SOURCE_DIR}/Geometries.cpp
    ${SOURCE_DIR}/Geometries.h
    ${SOURCE_DIR}/Utils.cpp
    ${SOURCE_DIR}/Utils.h
    ${SOURCE_DIR}/Types.h
)

# Set C++ standard version
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Link your project with SDL2 (assuming SDL2 provides CMake targets)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static SDL2::SDL2main bgfx bx bimg bimg_decode imgui)
# target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static SDL2::SDL2main imgui.cmake::imgui.cmake)

# Specify the output directory for the executable
# if (DEFINED BINARY_DIR)  # TODO: This is a hack to avoid error for ninja...
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
# endif()

# Set the working directory for debugging (for Visual Studio generator)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# get_target_property(tools_folder tools FOLDER)
# message(HELLO WORLD :: "${tools_folder}")
# get_target_property(tools_folder bgfx_tools_folder BGFX_TOOLS_FOLDER)
# message(HELLO WORLD :: "${tools_folder}")

if (WIN32)
    # copy the SDL2.dll to the same folder as the executable
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        VERBATIM)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/Assets/
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets/
        VERBATIM)
    # add_custom_command(
    #     TARGET ${PROJECT_NAME}
    #     POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #             ${SOURCE_DIR}/Temp/vs2022/Source/ThirdParty/bgfx.cmake/cmake/bgfx/Debug/shaderc.exe
    #             $<TARGET_FILE_DIR:${PROJECT_NAME}>
    #     VERBATIM)

    # ${SOURCE_DIR}/Temp/shaderc.exe ^
    # -f shader\v_simple.sc -o shader\build\v_simple.bin ^
    # --platform windows --type vertex --verbose -i ./ -p s_5_0
    
    # third-party\build\bin\shaderc.exe ^
    # -f shader\f_simple.sc -o shader\build\f_simple.bin ^
    # --platform windows --type fragment --verbose -i ./ -p s_5_0

endif ()
