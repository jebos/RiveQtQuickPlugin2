qt_add_executable(riveqtquick_unit_tests
    test_source_loading.cpp
    ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private/riveinspector.cpp
)

riveqt_set_runtime_output_directory(riveqtquick_unit_tests "tests-bin")

target_link_libraries(riveqtquick_unit_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

target_compile_definitions(riveqtquick_unit_tests
    PRIVATE
        RIVEQT_TEST_ASSET_DIR="${CMAKE_SOURCE_DIR}/tests/assets/rive"
        RIVEQT_TEST_HOSTED_ASSET_DIR="${CMAKE_SOURCE_DIR}/tests/assets/hosted"
)

if(WIN32)
    add_test(
        NAME riveqtquick_unit_tests
        COMMAND ${CMAKE_COMMAND}
            -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
            -DDEPLOY_MODE=$<IF:$<CONFIG:Debug>,--debug,--release>
            -DEXECUTABLE=$<TARGET_FILE:riveqtquick_unit_tests>
            -DEXECUTABLE_DIR=$<TARGET_FILE_DIR:riveqtquick_unit_tests>
            -DRUN_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deployed
            -DWORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/deployed
            -P ${CMAKE_SOURCE_DIR}/cmake/RunWithWinDeployQt.cmake
    )
else()
    add_test(
        NAME riveqtquick_unit_tests
        COMMAND $<TARGET_FILE:riveqtquick_unit_tests>
    )
endif()
