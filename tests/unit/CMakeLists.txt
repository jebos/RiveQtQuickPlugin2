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

if(RIVEQT_ENABLE_SOFTWARE)
    qt_add_executable(riveqtquick_software_paint_tests
        test_software_paint.cpp
        ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private/rendering/qpainter/qpainterrivepaint.cpp
        ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private/rendering/qpainter/qpainterriveshader.cpp
        ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private/rivelogging.cpp
    )
    riveqt_set_runtime_output_directory(riveqtquick_software_paint_tests "tests-bin")
    target_include_directories(riveqtquick_software_paint_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private
        ${CMAKE_SOURCE_DIR}/src/RiveQtQuick/private/rendering/qpainter
    )
    target_link_libraries(riveqtquick_software_paint_tests PRIVATE
        Qt6::Gui Qt6::Test rive_official
    )
    add_dependencies(riveqtquick_unit_tests riveqtquick_software_paint_tests)
    if(WIN32)
        add_test(NAME riveqtquick_software_paint_tests
            COMMAND ${CMAKE_COMMAND}
                -DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXECUTABLE}
                -DDEPLOY_MODE=$<IF:$<CONFIG:Debug>,--debug,--release>
                -DEXECUTABLE=$<TARGET_FILE:riveqtquick_software_paint_tests>
                -DEXECUTABLE_DIR=$<TARGET_FILE_DIR:riveqtquick_software_paint_tests>
                -DRUN_DEPLOY_DIR=${CMAKE_CURRENT_BINARY_DIR}/deployed-software-paint
                -DWORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/deployed-software-paint
                -P ${CMAKE_SOURCE_DIR}/cmake/RunWithWinDeployQt.cmake
        )
    else()
        add_test(NAME riveqtquick_software_paint_tests
            COMMAND $<TARGET_FILE:riveqtquick_software_paint_tests>
        )
    endif()
endif()

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
