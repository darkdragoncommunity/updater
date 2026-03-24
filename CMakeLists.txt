cmake_minimum_required(VERSION 3.14)
project(GameLauncher VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED) # Assume installed via vcpkg or system
find_package(glfw3 REQUIRED) # For window creation
find_package(ZLIB REQUIRED)

# ImGui (FetchContent or modify path as needed)
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG        docking
)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(imgui stb)

set(LAUNCHER_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(LAUNCHER_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(LAUNCHER_VERSION_BUILD_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION.txt")
set(LAUNCHER_VERSION_BUILD "1")
if(EXISTS "${LAUNCHER_VERSION_BUILD_FILE}")
    file(READ "${LAUNCHER_VERSION_BUILD_FILE}" LAUNCHER_VERSION_BUILD_RAW)
    string(STRIP "${LAUNCHER_VERSION_BUILD_RAW}" LAUNCHER_VERSION_BUILD_RAW)
    if(LAUNCHER_VERSION_BUILD_RAW MATCHES "^[0-9]+$")
        set(LAUNCHER_VERSION_BUILD "${LAUNCHER_VERSION_BUILD_RAW}")
    endif()
endif()
set(LAUNCHER_VERSION_REVISION 0)
set(LAUNCHER_VERSION_STRING "${LAUNCHER_VERSION_MAJOR}.${LAUNCHER_VERSION_MINOR}.${LAUNCHER_VERSION_BUILD}.${LAUNCHER_VERSION_REVISION}")

# Metadata Injection
set(LAUNCHER_PRODUCT_NAME "Game Launcher" CACHE STRING "Product Name")
set(LAUNCHER_COMPANY_NAME "Your Company" CACHE STRING "Company Name")
set(LAUNCHER_FILE_DESCRIPTION "Your Game Launcher" CACHE STRING "File Description")
set(LAUNCHER_INTERNAL_NAME "launcher.exe" CACHE STRING "Internal Name")
set(LAUNCHER_ORIGINAL_FILENAME "launcher.exe" CACHE STRING "Original Filename")
set(LAUNCHER_COPYRIGHT "Copyright (C) 2026 Your Company" CACHE STRING "Copyright")
set(LAUNCHER_ENCRYPTION_KEY "CHANGE_ME_DEFAULT_KEY_2026" CACHE STRING "XOR Encryption Key")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/launcher.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc
    @ONLY
)

add_executable(launcher
    src/main.cpp
    src/config.cpp
    src/language.cpp
    src/downloader.cpp
    src/manifest.cpp
    src/updater.cpp
    src/ui.cpp
    src/launcher_utils.cpp
    src/utils.cpp
    src/texture_utils.cpp
    src/extractor.cpp
    src/crypto_utils.cpp
    # ImGui sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

if(WIN32)
    target_sources(launcher PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/launcher.rc)
endif()
target_include_directories(launcher PRIVATE src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${stb_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(launcher PRIVATE CURL::libcurl OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json glfw ZLIB::ZLIB)

# Pass version and encryption key as compile definitions
target_compile_definitions(launcher PRIVATE 
    LAUNCHER_VERSION="${LAUNCHER_VERSION_STRING}"
    LAUNCHER_ENCRYPTION_KEY="${LAUNCHER_ENCRYPTION_KEY}"
)

# Bootstrap Updater Executable
add_executable(updater_bootstrap
    bootstrap/updater_bootstrap.cpp
)

# Windows-specific settings (hide console window for release builds)
if(WIN32)
    target_link_libraries(launcher PRIVATE opengl32)
    target_link_options(launcher PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    target_link_options(updater_bootstrap PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()
