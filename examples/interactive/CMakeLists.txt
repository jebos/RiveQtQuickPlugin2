qt_add_executable(riveqtquick_interactive
    main.cpp
    "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport.cpp"
)

riveqt_set_runtime_output_directory(riveqtquick_interactive "apps")

target_link_libraries(riveqtquick_interactive
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        RiveQtQuick
)
target_include_directories(riveqtquick_interactive
    PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
)
target_compile_definitions(riveqtquick_interactive
    PRIVATE
        RIVEQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
)

add_dependencies(riveqtquick_interactive RiveQtQuickPlugin)
riveqt_add_example_app_support(riveqtquick_interactive
    QML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
    IOS_MODULE_URI "RiveQtQuickExamples.Interactive"
    BUNDLE_SUFFIX "interactive"
    QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_FILES
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/hosted_image_file.riv"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/eve-317.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/one-45008.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/walle-370.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/walle.riv"
)
