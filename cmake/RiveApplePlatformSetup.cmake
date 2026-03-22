include_guard(GLOBAL)

function(_riveqt_is_ios_simulator_build_preproject out_var)
    set(result OFF)
    if(DEFINED CMAKE_OSX_SYSROOT)
        string(TOLOWER "${CMAKE_OSX_SYSROOT}" riveqt_osx_sysroot_lower)
        if(riveqt_osx_sysroot_lower MATCHES "iphonesimulator")
            set(result ON)
        endif()
    endif()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(_riveqt_detect_qt_ios_prefix_from_toolchain out_var)
    set(qt_ios_prefix "")
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        get_filename_component(toolchain_name "${CMAKE_TOOLCHAIN_FILE}" NAME)
        if(toolchain_name STREQUAL "qt.toolchain.cmake")
            get_filename_component(qt6_cmake_dir "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
            get_filename_component(qt6_lib_cmake_dir "${qt6_cmake_dir}" DIRECTORY)
            get_filename_component(qt_ios_lib_dir "${qt6_lib_cmake_dir}" DIRECTORY)
            get_filename_component(qt_ios_prefix "${qt_ios_lib_dir}" DIRECTORY)
        endif()
    endif()
    set(${out_var} "${qt_ios_prefix}" PARENT_SCOPE)
endfunction()

function(_riveqt_qt_archive_supports_arm64_ios_simulator archive_path out_var)
    set(result OFF)
    if(NOT EXISTS "${archive_path}")
        set(${out_var} "${result}" PARENT_SCOPE)
        return()
    endif()

    find_program(riveqt_lipo_executable NAMES lipo)
    find_program(riveqt_otool_executable NAMES otool)
    if(NOT riveqt_lipo_executable OR NOT riveqt_otool_executable)
        set(${out_var} "${result}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${riveqt_lipo_executable}" -archs "${archive_path}"
        RESULT_VARIABLE riveqt_lipo_result
        OUTPUT_VARIABLE riveqt_archive_archs
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT riveqt_lipo_result EQUAL 0)
        set(${out_var} "${result}" PARENT_SCOPE)
        return()
    endif()

    string(REPLACE " " ";" riveqt_archive_arch_list "${riveqt_archive_archs}")
    list(FIND riveqt_archive_arch_list "arm64" riveqt_arm64_index)
    if(riveqt_arm64_index EQUAL -1)
        set(${out_var} "${result}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${riveqt_otool_executable}" -arch arm64 -l "${archive_path}"
        RESULT_VARIABLE riveqt_otool_result
        OUTPUT_VARIABLE riveqt_load_commands
        ERROR_QUIET
    )
    if(riveqt_otool_result EQUAL 0
       AND riveqt_load_commands MATCHES "platform 7")
        # platform 7 is PLATFORM_IOSSIMULATOR.
        set(result ON)
    endif()

    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(_riveqt_detect_installed_ios_simulator_runtime_architectures known_out_var archs_out_var)
    set(known OFF)
    set(runtime_archs "")

    find_program(riveqt_xcrun_executable NAMES xcrun)
    if(riveqt_xcrun_executable)
        execute_process(
            COMMAND "${riveqt_xcrun_executable}" simctl runtime list -j
            RESULT_VARIABLE riveqt_simctl_result
            OUTPUT_VARIABLE riveqt_runtime_json
            ERROR_QUIET
        )
        if(riveqt_simctl_result EQUAL 0 AND riveqt_runtime_json)
            set(known ON)
            if(riveqt_runtime_json MATCHES "\"supportedArchitectures\"[^\[]*\\[[^]]*arm64")
                list(APPEND runtime_archs "arm64")
            endif()
            if(riveqt_runtime_json MATCHES "\"supportedArchitectures\"[^\[]*\\[[^]]*x86_64")
                list(APPEND runtime_archs "x86_64")
            endif()
            list(REMOVE_DUPLICATES runtime_archs)
        endif()
    endif()

    set(${known_out_var} "${known}" PARENT_SCOPE)
    set(${archs_out_var} "${runtime_archs}" PARENT_SCOPE)
endfunction()

function(_riveqt_format_arch_list input_archs out_var)
    set(archs ${input_archs})
    if(archs)
        list(JOIN archs ", " archs_display)
    else()
        set(archs_display "none detected")
    endif()
    set(${out_var} "${archs_display}" PARENT_SCOPE)
endfunction()

function(riveqt_prepare_apple_platform)
    _riveqt_is_ios_simulator_build_preproject(is_ios_simulator_build)
    if(NOT is_ios_simulator_build)
        return()
    endif()

    _riveqt_detect_qt_ios_prefix_from_toolchain(qt_ios_prefix)
    if(NOT qt_ios_prefix)
        return()
    endif()

    set(qt_arm64_simulator_supported ON)
    foreach(qt_framework_archive IN ITEMS
        "${qt_ios_prefix}/lib/QtQml.framework/QtQml"
        "${qt_ios_prefix}/lib/QtQuick.framework/QtQuick"
    )
        _riveqt_qt_archive_supports_arm64_ios_simulator(
            "${qt_framework_archive}" current_archive_supports_arm64_simulator)
        if(NOT current_archive_supports_arm64_simulator)
            set(qt_arm64_simulator_supported OFF)
            break()
        endif()
    endforeach()

    if(qt_arm64_simulator_supported)
        return()
    endif()

    _riveqt_detect_installed_ios_simulator_runtime_architectures(
        runtime_arch_query_known installed_ios_simulator_runtime_archs)
    _riveqt_format_arch_list(
        "${installed_ios_simulator_runtime_archs}" installed_runtime_archs_display)

    set(requested_archs ${CMAKE_OSX_ARCHITECTURES})
    list(FIND requested_archs "arm64" requested_arm64_index)
    list(FIND requested_archs "x86_64" requested_x86_64_index)
    list(FIND installed_ios_simulator_runtime_archs "x86_64" runtime_x86_64_index)

    string(CONCAT qt_arm64_failure_message
        "The Qt iOS kit at '${qt_ios_prefix}' does not provide arm64 iOS Simulator frameworks. "
        "Its arm64 QtQml/QtQuick archives are tagged for device iOS, so arm64 simulator builds "
        "will fail at link time with 'built for iOS'."
    )

    if(requested_arm64_index GREATER -1)
        message(FATAL_ERROR
            "${qt_arm64_failure_message}\n"
            "Reconfigure with -DCMAKE_OSX_ARCHITECTURES=x86_64 and run against an x86_64-capable "
            "iOS simulator runtime, or switch to a Qt iOS kit that ships arm64 simulator frameworks."
        )
    endif()

    if(NOT requested_archs)
        if(runtime_arch_query_known AND runtime_x86_64_index EQUAL -1)
            message(FATAL_ERROR
                "${qt_arm64_failure_message}\n"
                "This Mac's installed iOS simulator runtimes currently support: "
                "${installed_runtime_archs_display}.\n"
                "No x86_64-capable simulator runtime is available locally, so a runnable iOS "
                "Simulator build is not possible with this Qt kit.\n"
                "Use -DCMAKE_OSX_SYSROOT=iphoneos for device builds, install an x86_64-capable "
                "iOS simulator runtime, or install a Qt iOS kit with arm64 simulator support."
            )
        endif()

        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING
            "Architectures used for iOS Simulator builds." FORCE)
        message(STATUS
            "Qt iOS simulator frameworks in '${qt_ios_prefix}' are device-only for arm64; "
            "forcing CMAKE_OSX_ARCHITECTURES=x86_64."
        )

        if(runtime_arch_query_known)
            message(STATUS
                "Installed iOS simulator runtime architectures: ${installed_runtime_archs_display}."
            )
        else()
            message(WARNING
                "Could not verify installed iOS simulator runtime architectures on this host. "
                "The x86_64 simulator fallback build can compile, but it will only launch on an "
                "x86_64-capable iOS simulator runtime."
            )
        endif()
        return()
    endif()

    if(requested_x86_64_index GREATER -1
       AND runtime_arch_query_known
       AND runtime_x86_64_index EQUAL -1)
        message(WARNING
            "The configured iOS Simulator build uses x86_64, but the installed simulator "
            "runtimes on this Mac currently support: ${installed_runtime_archs_display}. "
            "The build can complete, but the app will not launch locally until you install an "
            "x86_64-capable iOS simulator runtime or switch to a Qt kit with arm64 simulator support."
        )
        return()
    endif()

    message(WARNING
        "${qt_arm64_failure_message}\n"
        "Current CMAKE_OSX_ARCHITECTURES='${CMAKE_OSX_ARCHITECTURES}'. "
        "If the simulator link fails, reconfigure with -DCMAKE_OSX_ARCHITECTURES=x86_64."
    )
endfunction()
