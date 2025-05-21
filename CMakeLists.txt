cmake_minimum_required(VERSION 3.30)
project(Cheat)

set(CMAKE_CXX_STANDARD 20)
enable_language(C ASM_MASM)
set(CMAKE_ASM_MASM_COMPILER ml64)
set(BUILD_SHARED_LIBS OFF)

set(MSVC_INCREMENTAL_DEFAULT OFF)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /EHc")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

set(WIN_SDK_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64")
list(APPEND CMAKE_LIBRARY_PATH "${WIN_SDK_PATH}")

add_custom_target(
        PreBuildStep ALL
        COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_definitions.py ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating custom function definitions and library definitions..."
)

add_library(${PROJECT_NAME} SHARED
        Entry.cpp
        Logger.cpp
        Logger.hpp
        Dependencies/termcolor.hpp
        OffsetsAndFunctions.hpp
        Roblox/TaskScheduler.cpp
        Roblox/TaskScheduler.hpp
        Execution/InternalTaskScheduler.cpp
        Execution/InternalTaskScheduler.hpp
        ShufflesAndVmValues.hpp
        Execution/BytecodeCompiler.cpp
        Execution/BytecodeCompiler.hpp
        Execution/LuauSecurity.cpp
        Execution/LuauSecurity.hpp
        Communication.cpp
        Communication.hpp
        Environment/Libraries/Globals.cpp
        Managers/CClosureManager.cpp
        Managers/CClosureManager.hpp
        Environment/EnvironmentManager.cpp
        Environment/EnvironmentManager.hpp
        Environment/Libraries/Globals.hpp
        Environment/Libraries/Closures.cpp
        Environment/Libraries/Closures.hpp
        Environment/Libraries/Instances.cpp
        Environment/Libraries/Instances.hpp
        Environment/Libraries/Misc.cpp
        Environment/Libraries/Misc.hpp
        Environment/Libraries/Metatable.cpp
        Environment/Libraries/Metatable.hpp
        Environment/Libraries/Cache.cpp
        Environment/Libraries/Cache.hpp
        Managers/FileSystemManager.cpp
        Managers/FileSystemManager.hpp
        Environment/Libraries/Filesystem.cpp
        Environment/Libraries/Filesystem.hpp
        Environment/Libraries/Debug.cpp
        Environment/Libraries/Debug.hpp
        Environment/Libraries/Script.cpp
        Environment/Libraries/Script.hpp
        Environment/Libraries/Crypt.cpp
        Environment/Libraries/Crypt.hpp
        Environment/Libraries/Websockets.cpp
        Environment/Libraries/Websockets.hpp
        Roblox/Instance.cpp
        Roblox/Instance.hpp
        ApplicationContext.hpp
        ApplicationContext.cpp
        Dependencies/oxorany.cpp
        Dependencies/oxorany.h
        Dependencies/oxorany_include.h

        Dependencies/Imgui/imconfig.h
        Dependencies/Imgui/imgui.cpp
        Dependencies/Imgui/imgui.h
        Dependencies/Imgui/imgui_draw.cpp
        Dependencies/Imgui/imgui_impl_dx11.cpp
        Dependencies/Imgui/imgui_impl_dx11.h
        Dependencies/Imgui/imgui_impl_win32.cpp
        Dependencies/Imgui/imgui_impl_win32.h
        Dependencies/Imgui/imgui_internal.h
        Dependencies/Imgui/imgui_tables.cpp
        Dependencies/Imgui/imgui_widgets.cpp
        Dependencies/Imgui/imstb_rectpack.h
        Dependencies/Imgui/imstb_textedit.h
        Dependencies/Imgui/imstb_truetype.h
        Managers/Render/RenderManager.cpp
        Managers/Render/RenderManager.hpp
        Environment/Libraries/Drawing.cpp
        Environment/Libraries/Drawing.hpp
        Dependencies/stb/stb_image.hpp
        Dependencies/stb/stb_image.cpp
        Dependencies/stb/stb_image_resize2.cpp
        Dependencies/stb/stb_image_resize2.hpp
        Managers/Render/Fonts.hpp
        Environment/ExploitObject.hpp
        Environment/ExploitObject.cpp
        Environment/ExploitObjects/Connection.cpp
        Environment/ExploitObjects/Connection.hpp
        Environment/ExploitObjects/Websocket.cpp
        Environment/ExploitObjects/Websocket.hpp
        Environment/ExploitObjects/Drawing.cpp
        Environment/ExploitObjects/Drawing.hpp
        Dependencies/WinReg.hpp
)

add_dependencies(${PROJECT_NAME} PreBuildStep)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies")

find_package(MbedTLS CONFIG REQUIRED)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/cryptopp")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Luau")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/cpr")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/IXWebsocket")
find_package(xxHash CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_library(CRYPT32_LIB crypt32 PATHS "${WIN_SDK_PATH}")
find_library(WLDAP32_LIB wldap32 PATHS "${WIN_SDK_PATH}")
find_library(NORMALIZ_LIB normaliz PATHS "${WIN_SDK_PATH}")
find_library(WS2_32_LIB ws2_32 PATHS "${WIN_SDK_PATH}")

target_link_libraries(${PROJECT_NAME} PRIVATE
        Luau.Compiler
        Luau.VM
        Luau.VM.Internals

        cryptopp::cryptopp
        xxHash::xxhash
        zstd::libzstd
        lz4::lz4
        cpr::cpr
        "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Curl/lib/libcurl_release.lib"
        ixwebsocket::ixwebsocket

        Ws2_32.lib
        Crypt32.lib
        Wldap32.lib
        Normaliz.lib
        d3d11.lib
)