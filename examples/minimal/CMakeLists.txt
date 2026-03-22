qt_add_executable(riveqtquick_minimal
    main.cpp
    "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport.cpp"
)

riveqt_set_runtime_output_directory(riveqtquick_minimal "apps")

target_link_libraries(riveqtquick_minimal
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        RiveQtQuick
)
target_include_directories(riveqtquick_minimal
    PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
)
target_compile_definitions(riveqtquick_minimal
    PRIVATE
        RIVEQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
)

add_dependencies(riveqtquick_minimal RiveQtQuickPlugin)
riveqt_add_example_app_support(riveqtquick_minimal
    QML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
    IOS_MODULE_URI "RiveQtQuickExamples.Minimal"
    BUNDLE_SUFFIX "minimal"
    QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_FILES
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/hosted_image_file.riv"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/eve-317.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/one-45008.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/walle-370.png"
        "${CMAKE_SOURCE_DIR}/tests/assets/hosted/walle.riv"
)
