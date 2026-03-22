qt_add_executable(riveqtquick_icon_grid
    main.cpp
    icongridmodel.cpp
    icongridmodel.h
    "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport.cpp"
    "${CMAKE_SOURCE_DIR}/3rdparty/rive-cpp/utils/no_op_factory.cpp"
)

riveqt_set_runtime_output_directory(riveqtquick_icon_grid "apps")

target_link_libraries(riveqtquick_icon_grid
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        RiveQtQuick
        Rive::Official
)
target_include_directories(riveqtquick_icon_grid
    PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
)
target_compile_definitions(riveqtquick_icon_grid
    PRIVATE
        RIVEQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
)

add_dependencies(riveqtquick_icon_grid RiveQtQuickPlugin)
riveqt_add_example_app_support(riveqtquick_icon_grid
    QML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
    IOS_MODULE_URI "RiveQtQuickExamples.IconGrid"
    BUNDLE_SUFFIX "icon-grid"
    QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_FILES
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/25691-49048-interactive-icon-set.riv"
)
