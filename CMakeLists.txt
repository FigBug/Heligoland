cmake_minimum_required(VERSION 3.16)

# macOS deployment target and universal binary - must be set before project()
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

project(Heligoland VERSION ${VERSION_STRING} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib options
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# Add raylib but exclude from install targets
add_subdirectory(modules/raylib EXCLUDE_FROM_ALL)

set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/Ship.cpp
    src/Turret.cpp
    src/Shell.cpp
    src/Player.cpp
    src/AIController.cpp
    src/Renderer.cpp
    src/Audio.cpp
)

if(WIN32)
    list(APPEND SOURCES src/WinMain.cpp)
endif()

set(HEADERS
    src/Config.h
    src/Game.h
    src/Ship.h
    src/Turret.h
    src/Shell.h
    src/Player.h
    src/AIController.h
    src/Renderer.h
    src/Audio.h
    src/Vec2.h
)

source_group("Source Files" FILES ${SOURCES} ${HEADERS})

if(APPLE)
    set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/icon.icns)

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCES} ${HEADERS})

    # Add icon if it exists
    if(EXISTS ${ICON_FILE})
        target_sources(${PROJECT_NAME} PRIVATE ${ICON_FILE})
        set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE "icon.icns")
    endif()
elseif(WIN32)
    set(RC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/icon.rc)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RC_FILE})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    # Disable linker manifest generation since we provide our own in icon.rc
    target_link_options(${PROJECT_NAME} PRIVATE "/MANIFEST:NO")
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Heligoland"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.heligoland.game"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# Copy assets to build directory (if they exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    if(APPLE)
        # For macOS, copy assets into the app bundle Resources folder
        file(GLOB ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
        foreach(ASSET_FILE ${ASSET_FILES})
            get_filename_component(ASSET_NAME ${ASSET_FILE} NAME)
            set_source_files_properties(${ASSET_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets")
            target_sources(${PROJECT_NAME} PRIVATE ${ASSET_FILE})
        endforeach()
    else()
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endif()

# =============================================================================
# CPack installer configuration
# =============================================================================
set(CPACK_PACKAGE_NAME "Heligoland")
set(CPACK_PACKAGE_VENDOR "Heligoland")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A naval combat game")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Heligoland")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

if(APPLE)
    # macOS: Create .pkg installer
    set(CPACK_GENERATOR "productbuild")
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.heligoland.game")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/Applications")

    # Install the app bundle to /Applications
    # Use generator expression to handle both single-config (Makefiles) and multi-config (Xcode) generators
    install(DIRECTORY "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>"
            DESTINATION "."
            USE_SOURCE_PERMISSIONS
            COMPONENT Runtime)

elseif(WIN32)
    # Windows: Create NSIS installer (.exe)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Heligoland")
    set(CPACK_NSIS_PACKAGE_NAME "Heligoland")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Heligoland.lnk' '$INSTDIR\\\\Heligoland.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Heligoland.lnk'")

    # Install executable and assets to root of install directory
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "." COMPONENT Runtime)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION "." COMPONENT Runtime)

else()
    # Linux: Create .deb package
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Heligoland")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libgl1, libasound2")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # Install to standard Linux locations
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin COMPONENT Runtime)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION share/heligoland COMPONENT Runtime)

    # Install desktop file for Linux
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/heligoland.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/heligoland.desktop"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/heligoland.desktop"
            DESTINATION share/applications COMPONENT Runtime)
endif()

include(CPack)
