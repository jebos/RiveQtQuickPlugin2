set(RIVE_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rive-cpp")
set(RIVE_HARFBUZZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/harfbuzz")
set(RIVE_SHEENBIDI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SheenBidi")
set(RIVE_YOGA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yoga")
set(RIVE_PLY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ply")
set(RIVE_LUAU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/luau")
set(RIVE_MINIAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniaudio")
set(RIVE_LIBHYDROGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libhydrogen")
set(RIVE_DIRECTX_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/directx-headers")
set(RIVE_GENERATED_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")

file(GLOB RIVE_VULKAN_SDK_DIRS LIST_DIRECTORIES true "C:/VulkanSDK/*")
if(RIVE_VULKAN_SDK_DIRS)
    list(SORT RIVE_VULKAN_SDK_DIRS COMPARE NATURAL ORDER DESCENDING)
    list(GET RIVE_VULKAN_SDK_DIRS 0 RIVE_VULKAN_SDK_DIR)
    set(RIVEQT_VMA_INCLUDE_DIR "${RIVE_VULKAN_SDK_DIR}/Include/vma")
endif()

unset(RIVE_GLSLANG_VALIDATOR_EXECUTABLE CACHE)
unset(RIVE_SPIRV_OPT_EXECUTABLE CACHE)
unset(RIVEQT_VULKAN_INCLUDE_DIR CACHE)
unset(RIVEQT_VULKAN_LIBRARY CACHE)

set(RIVEQT_ENABLE_D3D12 OFF)
if(WIN32)
    set(RIVEQT_ENABLE_D3D12 ON)
endif()
if(NOT DEFINED RIVEQT_ENABLE_METAL)
    set(RIVEQT_ENABLE_METAL OFF)
endif()

find_package(Vulkan QUIET)
find_program(RIVE_GLSLANG_VALIDATOR_EXECUTABLE
    NAMES glslangValidator.exe glslangValidator
    HINTS
        "${RIVE_VULKAN_SDK_DIR}/Bin"
)
find_program(RIVE_SPIRV_OPT_EXECUTABLE
    NAMES spirv-opt.exe spirv-opt
    HINTS
        "${RIVE_VULKAN_SDK_DIR}/Bin"
)

set(RIVEQT_VULKAN_INCLUDE_DIR "${Vulkan_INCLUDE_DIR}")
set(RIVEQT_VULKAN_LIBRARY "${Vulkan_LIBRARY}")
if(NOT RIVEQT_VULKAN_INCLUDE_DIR)
    find_path(RIVEQT_VULKAN_INCLUDE_DIR
        NAMES vulkan/vulkan.h
        HINTS
            "${RIVE_VULKAN_SDK_DIR}/Include"
    )
endif()
if(NOT RIVEQT_VULKAN_LIBRARY)
    find_library(RIVEQT_VULKAN_LIBRARY
        NAMES vulkan-1
        HINTS
            "${RIVE_VULKAN_SDK_DIR}/Lib"
    )
endif()

set(RIVEQT_ENABLE_VULKAN OFF)
if(RIVEQT_VULKAN_INCLUDE_DIR AND RIVEQT_VULKAN_LIBRARY AND
   RIVE_GLSLANG_VALIDATOR_EXECUTABLE AND RIVE_SPIRV_OPT_EXECUTABLE)
    set(RIVEQT_ENABLE_VULKAN ON)
endif()

if(NOT EXISTS "${RIVE_CPP_DIR}/include/rive/file.hpp")
    message(FATAL_ERROR "Missing vendored rive-cpp checkout in ${RIVE_CPP_DIR}.")
endif()
if(NOT EXISTS "${RIVE_LUAU_DIR}/VM/include/lua.h")
    message(FATAL_ERROR "Missing vendored Luau checkout in ${RIVE_LUAU_DIR}.")
endif()
set(RIVE_LIBHYDROGEN_SOURCE "${RIVE_LIBHYDROGEN_DIR}/libhydrogen.c")
if(NOT EXISTS "${RIVE_LIBHYDROGEN_SOURCE}")
    set(RIVE_LIBHYDROGEN_SOURCE "${RIVE_LIBHYDROGEN_DIR}/hydrogen.c")
endif()
if(NOT EXISTS "${RIVE_LIBHYDROGEN_SOURCE}")
    message(FATAL_ERROR "Missing vendored libhydrogen checkout in ${RIVE_LIBHYDROGEN_DIR}.")
endif()

find_program(RIVE_FXC_EXECUTABLE
    NAMES fxc.exe fxc
    HINTS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
)

if(WIN32 AND NOT RIVE_FXC_EXECUTABLE)
    message(FATAL_ERROR "fxc.exe was not found. Install the Windows 10/11 SDK shader tools.")
endif()

if(APPLE AND RIVEQT_ENABLE_METAL)
    find_program(RIVE_XCRUN_EXECUTABLE NAMES xcrun REQUIRED)

    execute_process(
        COMMAND "${RIVE_XCRUN_EXECUTABLE}" metal -help
        RESULT_VARIABLE RIVEQT_METAL_TOOL_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${RIVE_XCRUN_EXECUTABLE}" -find metallib
        RESULT_VARIABLE RIVEQT_METALLIB_TOOL_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )

    if(NOT RIVEQT_METAL_TOOL_RESULT EQUAL 0 OR NOT RIVEQT_METALLIB_TOOL_RESULT EQUAL 0)
        message(FATAL_ERROR
            "The Xcode Metal Toolchain is not available. "
            "Install it with: xcodebuild -downloadComponent MetalToolchain")
    endif()
endif()

set(RIVE_SHADER_STAMP "${RIVE_GENERATED_SHADER_DIR}/stamp.txt")
set(RIVE_SHADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_rive_shaders.py")
file(GLOB_RECURSE RIVE_SHADER_SOURCE_DEPENDS CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/renderer/src/shaders/*"
)
set(RIVE_SHADER_EXTRA_ARGS)
if(WIN32)
    list(APPEND RIVE_SHADER_EXTRA_ARGS --fxc=${RIVE_FXC_EXECUTABLE})
endif()
if(RIVEQT_ENABLE_VULKAN)
    list(APPEND RIVE_SHADER_EXTRA_ARGS
        --glslang-validator=${RIVE_GLSLANG_VALIDATOR_EXECUTABLE}
        --spirv-opt=${RIVE_SPIRV_OPT_EXECUTABLE}
    )
endif()
if(APPLE AND RIVEQT_ENABLE_METAL)
    string(TOLOWER "${CMAKE_OSX_SYSROOT}" RIVEQT_OSX_SYSROOT_LOWER)
    list(APPEND RIVE_SHADER_EXTRA_ARGS --metal)
    if(IOS)
        if(RIVEQT_OSX_SYSROOT_LOWER MATCHES "iphonesimulator")
            list(APPEND RIVE_SHADER_EXTRA_ARGS --metal-target ios-simulator)
        else()
            list(APPEND RIVE_SHADER_EXTRA_ARGS --metal-target ios)
        endif()
    else()
        list(APPEND RIVE_SHADER_EXTRA_ARGS --metal-target macos)
    endif()
endif()

add_custom_command(
    OUTPUT "${RIVE_SHADER_STAMP}"
    COMMAND
        "${Python3_EXECUTABLE}" "${RIVE_SHADER_SCRIPT}"
        --source-dir "${RIVE_CPP_DIR}/renderer/src/shaders"
        --out-dir "${RIVE_GENERATED_SHADER_DIR}"
        --ply-path "${RIVE_PLY_DIR}"
        ${RIVE_SHADER_EXTRA_ARGS}
    DEPENDS
        "${RIVE_SHADER_SCRIPT}"
        ${RIVE_SHADER_SOURCE_DEPENDS}
        "${RIVE_CPP_DIR}/renderer/src/shaders/minify.py"
        "${RIVE_CPP_DIR}/renderer/src/shaders/spirv_binary_to_header.py"
    COMMENT "Generating vendored Rive renderer shader headers"
    VERBATIM
)

add_custom_target(rive_renderer_shaders DEPENDS "${RIVE_SHADER_STAMP}")

add_library(rive_harfbuzz STATIC
    "${RIVE_HARFBUZZ_DIR}/src/harfbuzz.cc"
)
target_include_directories(rive_harfbuzz
    PUBLIC
        "${RIVE_HARFBUZZ_DIR}/src"
        "${RIVE_CPP_DIR}/dependencies"
)
target_compile_definitions(rive_harfbuzz
    PUBLIC
        HB_ONLY_ONE_SHAPER
        HAVE_OT
        HB_NO_FALLBACK_SHAPE
        HB_NO_WIN1256
        HB_NO_EXTERN_HELPERS
        HB_DISABLE_DEPRECATED
        HB_NO_BUFFER_SERIALIZE
        HB_NO_BUFFER_VERIFY
        HB_NO_BUFFER_MESSAGE
        HB_NO_SETLOCALE
        HB_NO_VERTICAL
        HB_NO_LAYOUT_COLLECT_GLYPHS
        HB_NO_LAYOUT_RARELY_USED
        HB_NO_LAYOUT_UNUSED
        HB_NO_OT_FONT_GLYPH_NAMES
        HB_NO_MMAP
        HB_NO_META
)
target_compile_features(rive_harfbuzz PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(rive_harfbuzz PRIVATE /bigobj /FS /Z7)
endif()

add_library(rive_sheenbidi STATIC
    "${RIVE_SHEENBIDI_DIR}/Source/SheenBidi.c"
)
target_include_directories(rive_sheenbidi
    PUBLIC
        "${RIVE_SHEENBIDI_DIR}/Headers"
        "${RIVE_SHEENBIDI_DIR}/Headers/SheenBidi"
    PRIVATE
        "${RIVE_SHEENBIDI_DIR}/Source"
)
target_compile_definitions(rive_sheenbidi PUBLIC SB_CONFIG_UNITY)

add_library(rive_yoga STATIC)
file(GLOB RIVE_YOGA_SOURCES CONFIGURE_DEPENDS
    "${RIVE_YOGA_DIR}/yoga/*.cpp"
    "${RIVE_YOGA_DIR}/yoga/event/*.cpp"
)
target_sources(rive_yoga PRIVATE ${RIVE_YOGA_SOURCES})
target_include_directories(rive_yoga
    PUBLIC
        "${RIVE_YOGA_DIR}"
        "${RIVE_CPP_DIR}/dependencies"
)
target_compile_definitions(rive_yoga PUBLIC YOGA_EXPORT=)

add_library(rive_miniaudio STATIC)
file(GLOB RIVE_MINIAUDIO_HEADERS CONFIGURE_DEPENDS
    "${RIVE_MINIAUDIO_DIR}/*.h"
)
if(APPLE OR IOS)
    file(GLOB RIVE_MINIAUDIO_SOURCES CONFIGURE_DEPENDS
        "${RIVE_CPP_DIR}/dependencies/miniaudio.m"
    )
else()
    file(GLOB RIVE_MINIAUDIO_SOURCES CONFIGURE_DEPENDS
        "${RIVE_MINIAUDIO_DIR}/*.c"
    )
endif()

target_sources(rive_miniaudio
    PRIVATE
        ${RIVE_MINIAUDIO_HEADERS}
        ${RIVE_MINIAUDIO_SOURCES}
)

target_include_directories(rive_miniaudio
    PUBLIC
        "${RIVE_MINIAUDIO_DIR}"
        "${RIVE_CPP_DIR}/dependencies"
)
if(IOS)
    target_link_libraries(rive_miniaudio
        PUBLIC
            "-framework AVFAudio"
    )
endif()

add_library(rive_luau_common STATIC)
file(GLOB RIVE_LUAU_COMMON_SOURCES CONFIGURE_DEPENDS
    "${RIVE_LUAU_DIR}/Common/src/*.cpp"
)
file(GLOB RIVE_LUAU_COMMON_HEADERS CONFIGURE_DEPENDS
    "${RIVE_LUAU_DIR}/Common/include/*.h"
    "${RIVE_LUAU_DIR}/Common/include/Luau/*.h"
)
target_sources(rive_luau_common
    PRIVATE
        ${RIVE_LUAU_COMMON_SOURCES}
        ${RIVE_LUAU_COMMON_HEADERS}
)
target_include_directories(rive_luau_common
    PUBLIC
        "${RIVE_LUAU_DIR}/Common/include"
)
target_compile_features(rive_luau_common PUBLIC cxx_std_17)
if(MSVC)
    target_compile_definitions(rive_luau_common PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rive_luau_common PRIVATE /bigobj /FS /Z7 /MP1)
endif()

add_library(rive_luau_vm STATIC)
file(GLOB RIVE_LUAU_VM_SOURCES CONFIGURE_DEPENDS
    "${RIVE_LUAU_DIR}/VM/src/*.cpp"
)
file(GLOB RIVE_LUAU_VM_HEADERS CONFIGURE_DEPENDS
    "${RIVE_LUAU_DIR}/VM/include/*.h"
    "${RIVE_LUAU_DIR}/VM/src/*.h"
)
target_sources(rive_luau_vm
    PRIVATE
        ${RIVE_LUAU_VM_SOURCES}
        ${RIVE_LUAU_VM_HEADERS}
)
target_include_directories(rive_luau_vm
    PUBLIC
        "${RIVE_LUAU_DIR}/VM/include"
        "${RIVE_LUAU_DIR}/Common/include"
    PRIVATE
        "${RIVE_LUAU_DIR}/VM/src"
)
target_link_libraries(rive_luau_vm PUBLIC rive_luau_common)
target_compile_definitions(rive_luau_vm PUBLIC LUA_USE_LONGJMP=1 RIVE_LUAU)
target_compile_features(rive_luau_vm PUBLIC cxx_std_17)
if(MSVC)
    target_compile_definitions(rive_luau_vm PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rive_luau_vm PRIVATE /bigobj /FS /Z7 /MP1)
    if(MSVC_VERSION GREATER_EQUAL 1924)
        set_source_files_properties(
            "${RIVE_LUAU_DIR}/VM/src/lvmexecute.cpp"
            PROPERTIES
                COMPILE_OPTIONS "/d2ssa-pre-"
        )
    endif()
else()
    target_compile_options(rive_luau_vm PRIVATE -fno-math-errno)
endif()

add_library(rive_libhydrogen STATIC
    "${RIVE_LIBHYDROGEN_SOURCE}"
)
target_include_directories(rive_libhydrogen
    PUBLIC
        "${RIVE_LIBHYDROGEN_DIR}"
)
if(MSVC)
    target_compile_options(rive_libhydrogen PRIVATE /FS /Z7 /W4 /wd4146 /wd4197 /wd4310)
    target_link_libraries(rive_libhydrogen PUBLIC advapi32)
endif()

add_library(rive_official STATIC)
add_dependencies(rive_official rive_renderer_shaders)

file(GLOB_RECURSE RIVE_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/include/*.hpp"
    "${RIVE_CPP_DIR}/src/*.cpp"
)

list(FILTER RIVE_RUNTIME_SOURCES EXCLUDE REGEX "/command_queue\\.cpp$")
list(FILTER RIVE_RUNTIME_SOURCES EXCLUDE REGEX "/command_server\\.cpp$")
file(GLOB_RECURSE RIVE_RENDERER_SOURCES CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/renderer/src/*.cpp"
)
file(GLOB_RECURSE RIVE_RENDERER_OBJCXX_SOURCES CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/renderer/src/*.mm"
)
file(GLOB_RECURSE RIVE_RENDERER_HEADERS CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/renderer/include/*.h"
    "${RIVE_CPP_DIR}/renderer/include/*.hpp"
)
file(GLOB RIVE_GLAD_SOURCES CONFIGURE_DEPENDS
    "${RIVE_CPP_DIR}/renderer/glad/src/*.c"
    "${RIVE_CPP_DIR}/renderer/glad/*.c"
)

list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/webgpu/")
if(NOT RIVEQT_ENABLE_OPENGL)
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/gl/")
endif()
if(NOT WIN32)
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/d3d/")
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/d3d11/")
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/d3d12/")
endif()
if(NOT RIVEQT_ENABLE_VULKAN)
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/vulkan/")
endif()
if(NOT RIVEQT_ENABLE_D3D12)
    list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/d3d12/")
endif()
if(NOT APPLE OR NOT RIVEQT_ENABLE_METAL)
    list(FILTER RIVE_RENDERER_OBJCXX_SOURCES EXCLUDE REGEX "/metal/")
endif()

list(FILTER RIVE_RENDERER_SOURCES EXCLUDE REGEX "/gl/load_gles_extensions\\.cpp$")

target_sources(rive_official
    PRIVATE
        ${RIVE_RUNTIME_SOURCES}
        ${RIVE_RENDERER_SOURCES}
        ${RIVE_RENDERER_OBJCXX_SOURCES}
        ${RIVE_GLAD_SOURCES}
        ${RIVE_RENDERER_HEADERS}
)

if(APPLE AND RIVEQT_ENABLE_METAL AND RIVE_RENDERER_OBJCXX_SOURCES)
    set_source_files_properties(${RIVE_RENDERER_OBJCXX_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "-fobjc-arc"
    )
endif()

target_include_directories(rive_official
    PUBLIC
        "${RIVE_CPP_DIR}/include"
        "${RIVE_CPP_DIR}/renderer/include"
        "${RIVE_CPP_DIR}/renderer/src"
        "${RIVE_CPP_DIR}/renderer/glad/include"
        "${RIVE_CPP_DIR}/renderer/glad"
        "${RIVE_CPP_DIR}/tests/include"
        "${RIVE_DIRECTX_HEADERS_DIR}/include"
        "${RIVE_DIRECTX_HEADERS_DIR}/include/directx"
        $<$<BOOL:${RIVEQT_ENABLE_VULKAN}>:${RIVEQT_VULKAN_INCLUDE_DIR}>
        $<$<BOOL:${RIVEQT_ENABLE_VULKAN}>:${RIVEQT_VMA_INCLUDE_DIR}>
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${RIVE_HARFBUZZ_DIR}/src"
        "${RIVE_SHEENBIDI_DIR}/Headers"
        "${RIVE_YOGA_DIR}"
        "${RIVE_CPP_DIR}/dependencies"
)

target_compile_definitions(rive_official
    PUBLIC
        WITH_RIVE_TEXT
        WITH_RIVE_LAYOUT
        WITH_RIVE_SCRIPTING
        WITH_RIVE_AUDIO
        _RIVE_INTERNAL_
        YOGA_EXPORT=
        $<$<BOOL:${RIVEQT_ENABLE_OPENGL}>:RIVE_DESKTOP_GL>
)

if(IOS)
    if(RIVEQT_OSX_SYSROOT_LOWER MATCHES "iphonesimulator")
        target_compile_definitions(rive_official PUBLIC RIVE_IOS_SIMULATOR)
    else()
        target_compile_definitions(rive_official PUBLIC RIVE_IOS)
    endif()
endif()

if(ANDROID)
    target_compile_definitions(rive_official
        PUBLIC
            VMA_STATIC_VULKAN_FUNCTIONS=0
            VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    )
endif()

if(WIN32)
    target_compile_definitions(rive_official PUBLIC RIVE_WINDOWS _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(rive_official PUBLIC d3d11 d3d12 dxgi dxguid d3dcompiler opengl32)
endif()

if(RIVEQT_ENABLE_VULKAN)
    target_compile_definitions(rive_official PUBLIC RIVE_VULKAN)
    if(TARGET Vulkan::Vulkan)
        target_link_libraries(rive_official PUBLIC Vulkan::Vulkan)
    else()
        target_link_libraries(rive_official PUBLIC "${RIVEQT_VULKAN_LIBRARY}")
    endif()
endif()
if(APPLE AND RIVEQT_ENABLE_METAL)
    target_link_libraries(rive_official
        PUBLIC
            "-framework Metal"
            "-framework QuartzCore"
            "-framework Foundation"
    )
endif()

target_link_libraries(rive_official
    PUBLIC
        rive_harfbuzz
        rive_sheenbidi
        rive_yoga
        rive_luau_vm
        rive_libhydrogen
        rive_miniaudio
)
target_compile_features(rive_official PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(rive_official PRIVATE /bigobj /FS /Z7 /MP1)
endif()

add_library(Rive::Official ALIAS rive_official)
