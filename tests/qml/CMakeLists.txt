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
)

add_dependencies(riveqtquick_qml_tests RiveQtQuickPlugin)
set(RIVEQT_QML_TEST_APIS)
if(WIN32)
    list(APPEND RIVEQT_QML_TEST_APIS d3d11)
    if(RIVEQT_ENABLE_D3D12)
        list(APPEND RIVEQT_QML_TEST_APIS d3d12)
    endif()
    if(RIVEQT_ENABLE_VULKAN)
        list(APPEND RIVEQT_QML_TEST_APIS vulkan)
    endif()
elseif(APPLE AND RIVEQT_ENABLE_METAL)
    list(APPEND RIVEQT_QML_TEST_APIS metal)
elseif(RIVEQT_ENABLE_VULKAN)
    list(APPEND RIVEQT_QML_TEST_APIS vulkan)
endif()

foreach(riveqt_backend IN LISTS RIVEQT_QML_TEST_APIS)
    if(riveqt_backend STREQUAL "d3d11")
        set(riveqt_test_name riveqtquick_qml_tests)
    else()
        set(riveqt_test_name riveqtquick_qml_tests_${riveqt_backend})
    endif()

    if(WIN32)
        add_test(
            NAME ${riveqt_test_name}
            COMMAND ${CMAKE_COMMAND}
                -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
                -DDEPLOY_MODE=$<IF:$<CONFIG:Debug>,--debug,--release>
                -DEXECUTABLE=$<TARGET_FILE:riveqtquick_qml_tests>
                -DEXECUTABLE_DIR=$<TARGET_FILE_DIR:riveqtquick_qml_tests>
                -DRUN_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deployed
                -DWORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/deployed
                -DQMLDIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DLOCAL_QML_MODULE_NAME=RiveQtQuick
                -DLOCAL_QML_MODULE_DIR=${CMAKE_BINARY_DIR}/src/RiveQtQuick
                -DLOCAL_QML_PLUGIN=$<TARGET_FILE:RiveQtQuickPlugin>
                -DEXTRA_RUNTIME_FILES=$<TARGET_FILE:RiveQtQuick>|${CMAKE_BINARY_DIR}/Debug/dxcompiler.dll|${CMAKE_BINARY_DIR}/Debug/dxil.dll
                "-DEXECUTABLE_ARGS=--graphics-api ${riveqt_backend}"
                -P ${CMAKE_SOURCE_DIR}/cmake/RunWithWinDeployQt.cmake
        )
    else()
        add_test(
            NAME ${riveqt_test_name}
            COMMAND $<TARGET_FILE:riveqtquick_qml_tests> --graphics-api ${riveqt_backend}
        )
    endif()
endforeach()
