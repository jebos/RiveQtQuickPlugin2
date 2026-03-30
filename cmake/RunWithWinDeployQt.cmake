if(NOT DEFINED EXECUTABLE OR EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "EXECUTABLE is required")
endif()

set(executable_to_run "${EXECUTABLE}")
set(executable_dir "${EXECUTABLE_DIR}")

if(DEFINED RUN_DEPLOY_DIR AND NOT RUN_DEPLOY_DIR STREQUAL "")
    get_filename_component(executable_name "${EXECUTABLE}" NAME)
    file(MAKE_DIRECTORY "${RUN_DEPLOY_DIR}")
    file(COPY "${EXECUTABLE}" DESTINATION "${RUN_DEPLOY_DIR}")
    set(executable_to_run "${RUN_DEPLOY_DIR}/${executable_name}")
    set(executable_dir "${RUN_DEPLOY_DIR}")
endif()

set(default_runtime_candidates
    "${EXECUTABLE_DIR}/dxcompiler.dll"
    "${EXECUTABLE_DIR}/dxil.dll"
    "${EXECUTABLE_DIR}/D3Dcompiler_47.dll"
)
foreach(default_runtime_file IN LISTS default_runtime_candidates)
    if(EXISTS "${default_runtime_file}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${default_runtime_file}" "${executable_dir}"
        )
    endif()
endforeach()

if(DEFINED LOCAL_QML_MODULE_NAME AND NOT LOCAL_QML_MODULE_NAME STREQUAL "" AND
   DEFINED LOCAL_QML_MODULE_DIR AND NOT LOCAL_QML_MODULE_DIR STREQUAL "")
    file(MAKE_DIRECTORY "${executable_dir}/qml/${LOCAL_QML_MODULE_NAME}")
    file(COPY "${LOCAL_QML_MODULE_DIR}/qmldir"
         DESTINATION "${executable_dir}/qml/${LOCAL_QML_MODULE_NAME}")
    file(COPY "${LOCAL_QML_MODULE_DIR}/${LOCAL_QML_MODULE_NAME}.qmltypes"
         DESTINATION "${executable_dir}/qml/${LOCAL_QML_MODULE_NAME}")
    if(DEFINED LOCAL_QML_PLUGIN AND NOT LOCAL_QML_PLUGIN STREQUAL "")
        file(COPY "${LOCAL_QML_PLUGIN}"
             DESTINATION "${executable_dir}/qml/${LOCAL_QML_MODULE_NAME}")
    endif()
endif()

if(DEFINED PLATFORM_PLUGIN AND NOT PLATFORM_PLUGIN STREQUAL "")
    file(MAKE_DIRECTORY "${executable_dir}/platforms")
    file(COPY "${PLATFORM_PLUGIN}" DESTINATION "${executable_dir}/platforms")
endif()

if(DEFINED EXTRA_RUNTIME_FILES AND NOT EXTRA_RUNTIME_FILES STREQUAL "")
    string(REPLACE "|" ";" extra_runtime_files "${EXTRA_RUNTIME_FILES}")
    foreach(extra_file IN LISTS extra_runtime_files)
        if(EXISTS "${extra_file}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${extra_file}" "${executable_dir}"
            )
        endif()
    endforeach()
endif()

if(DEFINED WINDEPLOYQT_EXECUTABLE AND WINDEPLOYQT_EXECUTABLE MATCHES "-NOTFOUND$")
    unset(WINDEPLOYQT_EXECUTABLE)
endif()

if(WIN32 AND DEFINED WINDEPLOYQT_EXECUTABLE AND NOT WINDEPLOYQT_EXECUTABLE STREQUAL "")
    set(deploy_command
        "${WINDEPLOYQT_EXECUTABLE}"
        "--force"
        "--no-compiler-runtime"
        "--dir" "${executable_dir}"
    )
endif()

if(DEFINED DEPLOY_MODE AND NOT DEPLOY_MODE STREQUAL "")
    list(APPEND deploy_command "${DEPLOY_MODE}")
endif()

if(QMLDIR)
    list(APPEND deploy_command "--qmldir" "${QMLDIR}")
endif()

if(deploy_command)
    list(APPEND deploy_command "${executable_to_run}")
    execute_process(
        COMMAND ${deploy_command}
        RESULT_VARIABLE deploy_result
    )
    if(NOT deploy_result EQUAL 0)
        message(FATAL_ERROR "windeployqt failed with exit code ${deploy_result}")
    endif()
endif()

if(DEFINED ENV_QT_QPA_PLATFORM AND NOT ENV_QT_QPA_PLATFORM STREQUAL "")
    set(ENV{QT_QPA_PLATFORM} "${ENV_QT_QPA_PLATFORM}")
endif()

set(run_command "${executable_to_run}")
if(DEFINED EXECUTABLE_ARGS AND NOT EXECUTABLE_ARGS STREQUAL "")
    separate_arguments(parsed_args NATIVE_COMMAND "${EXECUTABLE_ARGS}")
    list(APPEND run_command ${parsed_args})
endif()

if(WIN32)
    set(run_stdout_file "${executable_dir}/run-stdout.txt")
    set(run_stderr_file "${executable_dir}/run-stderr.txt")
    file(REMOVE "${run_stdout_file}" "${run_stderr_file}")

    set(ps_argument_list)
    foreach(run_arg IN LISTS parsed_args)
        string(REPLACE "'" "''" escaped_run_arg "${run_arg}")
        string(APPEND ps_argument_list "    '${escaped_run_arg}'\n")
    endforeach()
    set(launcher_argument_clause)
    if(NOT "${ps_argument_list}" STREQUAL "")
        set(launcher_argument_clause "-ArgumentList @(\n${ps_argument_list}) ")
    endif()

    string(REPLACE "'" "''" escaped_executable_to_run "${executable_to_run}")
    string(REPLACE "'" "''" escaped_working_directory "${WORKING_DIRECTORY}")
    string(REPLACE "'" "''" escaped_run_stdout_file "${run_stdout_file}")
    string(REPLACE "'" "''" escaped_run_stderr_file "${run_stderr_file}")
    set(launcher_timeout_ms -1)
    if(DEFINED RUN_TIMEOUT_SECONDS AND NOT RUN_TIMEOUT_SECONDS STREQUAL "")
        math(EXPR launcher_timeout_ms "${RUN_TIMEOUT_SECONDS} * 1000")
    endif()

    set(win_launcher_script "${executable_dir}/run-with-windeployqt.ps1")
    set(launcher_script_content "$ErrorActionPreference = 'Stop'\n")
    string(APPEND launcher_script_content
        "$process = Start-Process -FilePath '${escaped_executable_to_run}' "
        "${launcher_argument_clause}"
        "-WorkingDirectory '${escaped_working_directory}' "
        "-PassThru "
        "-RedirectStandardOutput '${escaped_run_stdout_file}' "
        "-RedirectStandardError '${escaped_run_stderr_file}'\n"
    )
    string(APPEND launcher_script_content
        "if (${launcher_timeout_ms} -ge 0) {\n"
        "    if (-not \$process.WaitForExit(${launcher_timeout_ms})) {\n"
        "        try {\n"
        "            \$process.Kill(\$true)\n"
        "        } catch {\n"
        "        }\n"
        "        exit 124\n"
        "    }\n"
        "} else {\n"
        "    \$process.WaitForExit()\n"
        "}\n"
        "exit \$process.ExitCode\n"
    )
    file(WRITE "${win_launcher_script}" "${launcher_script_content}")

    execute_process(
        COMMAND powershell -NoLogo -NonInteractive -ExecutionPolicy Bypass -File "${win_launcher_script}"
        RESULT_VARIABLE run_result
    )

    if(EXISTS "${run_stdout_file}")
        file(READ "${run_stdout_file}" run_output)
    endif()
    if(EXISTS "${run_stderr_file}")
        file(READ "${run_stderr_file}" run_error)
    endif()
else()
    set(run_process_args
        COMMAND ${run_command}
        WORKING_DIRECTORY "${WORKING_DIRECTORY}"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_output
        ERROR_VARIABLE run_error
    )
    if(DEFINED RUN_TIMEOUT_SECONDS AND NOT RUN_TIMEOUT_SECONDS STREQUAL "")
        list(APPEND run_process_args TIMEOUT "${RUN_TIMEOUT_SECONDS}")
    endif()

    execute_process(${run_process_args})
endif()

if(DEFINED ALLOW_TIMEOUT_SUCCESS AND ALLOW_TIMEOUT_SUCCESS)
    string(TOLOWER "${run_result}" run_result_lower)
    if(run_result_lower MATCHES "timeout" OR "${run_result}" STREQUAL "124")
        set(run_result 0)
    endif()
endif()

if(NOT run_result EQUAL 0)
    if(NOT run_output STREQUAL "")
        message(STATUS "Executable stdout:\n${run_output}")
    endif()
    if(NOT run_error STREQUAL "")
        message(STATUS "Executable stderr:\n${run_error}")
    endif()
    message(FATAL_ERROR "Test executable failed with exit code ${run_result}")
endif()
