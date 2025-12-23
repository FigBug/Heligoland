cmake_minimum_required(VERSION 3.16)
project(Heligoland VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

# SDL3 options
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

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

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)

# Platform-specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm imm32 version setupapi)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
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
