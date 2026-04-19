qt_add_executable(riveqtquick_audio
    main.cpp
    "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport.cpp"
)

if(IOS)
    target_sources(riveqtquick_audio
        PRIVATE
            "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport_ios.mm"
    )
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/examples/common/exampleappsupport_ios.mm"
        PROPERTIES
            COMPILE_OPTIONS "-fobjc-arc"
    )
endif()

riveqt_set_runtime_output_directory(riveqtquick_audio "apps")

target_link_libraries(riveqtquick_audio
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        RiveQtQuick
)
target_include_directories(riveqtquick_audio
    PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
)
target_compile_definitions(riveqtquick_audio
    PRIVATE
        RIVEQT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RIVEQT_BUILD_QML_IMPORT_DIR="${CMAKE_BINARY_DIR}/src"
)

add_dependencies(riveqtquick_audio RiveQtQuickPlugin)
riveqt_add_example_app_support(riveqtquick_audio
    QML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Main.qml"
    IOS_MODULE_URI "RiveQtQuickExamples.Audio"
    BUNDLE_SUFFIX "audio"
    QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_FILES
        "${CMAKE_SOURCE_DIR}/tests/assets/rive/26986-50750-scripted-audio-play-stop-volume.riv"
)
