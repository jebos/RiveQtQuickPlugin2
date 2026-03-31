qt_add_executable(riveqtquick_qml_tests
    test_qml_imports.cpp
)

riveqt_set_runtime_output_directory(riveqtquick_qml_tests "tests-bin")

target_link_libraries(riveqtquick_qml_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        RiveQtQuick
)

target_compile_definitions(riveqtquick_qml_tests
    PRIVATE
        RIVEQT_TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/tests/assets/rive"
        RIVEQT_TEST_HOSTED_ASSET_DIR="${CMAKE_SOURCE_DIR}/tests/assets/hosted"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
        RIVEQT_VENDOR_RIVE_TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/3rdparty/rive-cpp/tests/unit_tests/assets"
)

add_dependencies(riveqtquick_qml_tests RiveQtQuickPlugin)
set(RIVEQT_QML_TEST_APIS)
if(WIN32)
    list(APPEND RIVEQT_QML_TEST_APIS d3d11)
    if(RIVEQT_ENABLE_OPENGL)
        list(APPEND RIVEQT_QML_TEST_APIS opengl)
    endif()
    if(RIVEQT_ENABLE_D3D12)
        list(APPEND RIVEQT_QML_TEST_APIS d3d12)
    endif()
    if(RIVEQT_ENABLE_VULKAN)
        list(APPEND RIVEQT_QML_TEST_APIS vulkan)
    endif()
    if(NOT IOS AND RIVEQT_ENABLE_SOFTWARE)
        list(APPEND RIVEQT_QML_TEST_APIS software)
    endif()
elseif(APPLE)
    if(RIVEQT_ENABLE_METAL)
        list(APPEND RIVEQT_QML_TEST_APIS metal)
    endif()
    if(NOT IOS AND RIVEQT_ENABLE_SOFTWARE)
        list(APPEND RIVEQT_QML_TEST_APIS software)
    endif()
else()
    if(RIVEQT_ENABLE_VULKAN)
        list(APPEND RIVEQT_QML_TEST_APIS vulkan)
    endif()
    if(RIVEQT_ENABLE_OPENGL)
        list(APPEND RIVEQT_QML_TEST_APIS opengl)
    endif()
    if(NOT IOS AND RIVEQT_ENABLE_SOFTWARE)
        list(APPEND RIVEQT_QML_TEST_APIS software)
    endif()
endif()

set(RIVEQT_QML_REFERENCE_BACKEND "")
if(APPLE AND RIVEQT_ENABLE_METAL)
    set(RIVEQT_QML_REFERENCE_BACKEND metal)
endif()

if(RIVEQT_ENABLE_SOFTWARE AND RIVEQT_QML_REFERENCE_BACKEND)
    set(RIVEQT_QML_REFERENCE_CAPTURE_TEST riveqtquick_qml_tests_${RIVEQT_QML_REFERENCE_BACKEND}_capture)
    set(RIVEQT_QML_REFERENCE_CAPTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/qml-baselines/${RIVEQT_QML_REFERENCE_BACKEND}")
    if(WIN32)
        add_test(
            NAME ${RIVEQT_QML_REFERENCE_CAPTURE_TEST}
            COMMAND ${CMAKE_COMMAND}
                -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
                -DDEPLOY_MODE=$<IF:$<CONFIG:Debug>,--debug,--release>
                -DEXECUTABLE=$<TARGET_FILE:riveqtquick_qml_tests>
                -DEXECUTABLE_DIR=$<TARGET_FILE_DIR:riveqtquick_qml_tests>
                -DRUN_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deployed-${RIVEQT_QML_REFERENCE_BACKEND}-capture
                -DWORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/deployed-${RIVEQT_QML_REFERENCE_BACKEND}-capture
                -DQMLDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DLOCAL_QML_MODULE_NAME=RiveQtQuick
                -DLOCAL_QML_MODULE_DIR=${CMAKE_BINARY_DIR}/src/RiveQtQuick
                -DLOCAL_QML_PLUGIN=$<TARGET_FILE:RiveQtQuickPlugin>
                -DEXTRA_RUNTIME_FILES=$<TARGET_FILE:RiveQtQuick>|${CMAKE_BINARY_DIR}/Debug/dxcompiler.dll|${CMAKE_BINARY_DIR}/Debug/dxil.dll
                "-DEXECUTABLE_ARGS=--graphics-api ${RIVEQT_QML_REFERENCE_BACKEND} --capture-dir ${RIVEQT_QML_REFERENCE_CAPTURE_DIR}"
                -P ${CMAKE_SOURCE_DIR}/cmake/RunWithWinDeployQt.cmake
        )
    else()
        add_test(
            NAME ${RIVEQT_QML_REFERENCE_CAPTURE_TEST}
            COMMAND $<TARGET_FILE:riveqtquick_qml_tests> --graphics-api ${RIVEQT_QML_REFERENCE_BACKEND} --capture-dir ${RIVEQT_QML_REFERENCE_CAPTURE_DIR}
        )
    endif()
endif()

foreach(riveqt_backend IN LISTS RIVEQT_QML_TEST_APIS)
    if(riveqt_backend STREQUAL "d3d11")
        set(riveqt_test_name riveqtquick_qml_tests)
    else()
        set(riveqt_test_name riveqtquick_qml_tests_${riveqt_backend})
    endif()

    set(riveqt_test_args --graphics-api ${riveqt_backend})
    if(riveqt_backend STREQUAL "software" AND RIVEQT_QML_REFERENCE_BACKEND)
        list(APPEND riveqt_test_args --compare-dir ${RIVEQT_QML_REFERENCE_CAPTURE_DIR})
    endif()
    string(JOIN " " riveqt_test_args_string ${riveqt_test_args})

    if(WIN32)
        set(riveqt_deploy_dir "${CMAKE_CURRENT_BINARY_DIR}/deployed-${riveqt_backend}")
        add_test(
            NAME ${riveqt_test_name}
            COMMAND ${CMAKE_COMMAND}
                -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
                -DDEPLOY_MODE=$<IF:$<CONFIG:Debug>,--debug,--release>
                -DEXECUTABLE=$<TARGET_FILE:riveqtquick_qml_tests>
                -DEXECUTABLE_DIR=$<TARGET_FILE_DIR:riveqtquick_qml_tests>
                -DRUN_DEPLOY_DIR=${riveqt_deploy_dir}
                -DWORKING_DIRECTORY=${riveqt_deploy_dir}
                -DQMLDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DLOCAL_QML_MODULE_NAME=RiveQtQuick
                -DLOCAL_QML_MODULE_DIR=${CMAKE_BINARY_DIR}/src/RiveQtQuick
                -DLOCAL_QML_PLUGIN=$<TARGET_FILE:RiveQtQuickPlugin>
                -DEXTRA_RUNTIME_FILES=$<TARGET_FILE:RiveQtQuick>|${CMAKE_BINARY_DIR}/Debug/dxcompiler.dll|${CMAKE_BINARY_DIR}/Debug/dxil.dll
                "-DEXECUTABLE_ARGS=${riveqt_test_args_string}"
                -P ${CMAKE_SOURCE_DIR}/cmake/RunWithWinDeployQt.cmake
        )
    else()
        add_test(
            NAME ${riveqt_test_name}
            COMMAND $<TARGET_FILE:riveqtquick_qml_tests> ${riveqt_test_args}
        )
    endif()
endforeach()

if(RIVEQT_ENABLE_SOFTWARE AND RIVEQT_QML_REFERENCE_BACKEND)
    set_tests_properties(riveqtquick_qml_tests_software PROPERTIES DEPENDS ${RIVEQT_QML_REFERENCE_CAPTURE_TEST})
endif()
