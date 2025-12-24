cmake_minimum_required(VERSION 3.16)

# macOS deployment target and universal binary - must be set before project()
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")

project(Heligoland VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 options
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

# Disable XTEST (causes issues on GitHub Actions Linux)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

add_subdirectory(modules/SDL)

set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/Ship.cpp
    src/Turret.cpp
    src/Shell.cpp
    src/Player.cpp
    src/AIController.cpp
    src/Renderer.cpp
)

set(HEADERS
    include/Game.h
    include/Ship.h
    include/Turret.h
    include/Shell.h
    include/Player.h
    include/AIController.h
    include/Renderer.h
    include/Vec2.h
)

if(APPLE)
    # Generate icns from jpg for macOS app icon at configure time
    set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
    set(ICONSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/Heligoland.iconset)
    set(ICON_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Heligoland.icns)

    # Generate the icon at configure time so it exists before build
    if(EXISTS ${ICON_SOURCE})
        message(STATUS "Generating macOS app icon from ${ICON_SOURCE}")
        file(MAKE_DIRECTORY ${ICONSET_DIR})
        execute_process(COMMAND sips -s format png -z 16 16 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_16x16.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 32 32 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_16x16@2x.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 32 32 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_32x32.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 64 64 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_32x32@2x.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 128 128 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_128x128.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 256 256 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_128x128@2x.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 256 256 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_256x256.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 512 512 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_256x256@2x.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 512 512 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_512x512.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND sips -s format png -z 1024 1024 ${ICON_SOURCE} --out ${ICONSET_DIR}/icon_512x512@2x.png OUTPUT_QUIET ERROR_QUIET)
        execute_process(COMMAND iconutil -c icns ${ICONSET_DIR} -o ${ICON_OUTPUT} OUTPUT_QUIET ERROR_QUIET)
    endif()

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS})

    # Add icon if it was successfully generated
    if(EXISTS ${ICON_OUTPUT})
        message(STATUS "Adding app icon: ${ICON_OUTPUT}")
        target_sources(${PROJECT_NAME} PRIVATE ${ICON_OUTPUT})
        set_source_files_properties(${ICON_OUTPUT} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE "Heligoland.icns")
    endif()
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)

# Platform-specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm imm32 version setupapi)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Heligoland"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.heligoland.game"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "Heligoland.icns"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreVideo"
        "-framework AudioToolbox"
        "-framework Carbon"
        "-framework ForceFeedback"
        "-framework GameController"
        "-framework CoreHaptics"
    )
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
endif()

# Copy assets to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
