qt_add_executable(riveqtquick_marketplace_demo
    main.cpp
    marketplacecatalog.cpp
    marketplacecatalog.h
    "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport.cpp"
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_sources(riveqtquick_marketplace_demo
        PRIVATE
            marketplaceiosnetwork.h
            marketplaceiosnetwork.mm
    )
    set_source_files_properties(marketplaceiosnetwork.mm
        PROPERTIES
            COMPILE_OPTIONS "-fobjc-arc"
    )
endif()

riveqt_set_runtime_output_directory(riveqtquick_marketplace_demo "apps")

target_link_libraries(riveqtquick_marketplace_demo
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        RiveQtQuick
)
if(APPLE)
    target_link_libraries(riveqtquick_marketplace_demo PRIVATE "-framework Foundation")
endif()
target_include_directories(riveqtquick_marketplace_demo
    PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
)
target_compile_definitions(riveqtquick_marketplace_demo
    PRIVATE
        RIVEQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
)

add_dependencies(riveqtquick_marketplace_demo RiveQtQuickPlugin)
riveqt_add_example_app_support(riveqtquick_marketplace_demo
    QML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
    IOS_MODULE_URI "RiveQtQuickExamples.MarketplaceDemo"
    BUNDLE_SUFFIX "marketplace-demo"
    QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/starter_catalog.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/posters/hello_world.svg"
        "${CMAKE_CURRENT_SOURCE_DIR}/posters/interactive_icons.svg"
        "${CMAKE_CURRENT_SOURCE_DIR}/posters/state_machine_triggers.svg"
        "${CMAKE_CURRENT_SOURCE_DIR}/posters/data_binding.svg"
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/hello_world.riv"
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/25691-49048-interactive-icon-set.riv"
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/state_machine_triggers.riv"
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/data_binding_test.riv"
)
