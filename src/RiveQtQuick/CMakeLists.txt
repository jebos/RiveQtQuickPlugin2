set(RIVEQTQUICK_SOURCES
    rivebinding.cpp
    riveitem.cpp
    riveqtquickplugin.cpp
    private/rivebackendbridge.cpp
    private/rivelogging.cpp
    private/rivefileassetloader.cpp
    private/riveinspector.cpp
    private/riverendernode.cpp
)

set(RIVEQTQUICK_HEADERS
    rivebinding.h
    riveitem.h
    private/rivebackendbridge.h
    private/rivelogging.h
    private/rivefileassetloader.h
    private/riveinspector.h
    private/riverendernode.h
    private/riverenderstate.h
)

if(WIN32)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rived3d11bridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rived3d11bridge.h
    )
endif()

if(RIVEQT_ENABLE_D3D12)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rived3d12bridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rived3d12bridge.h
    )
endif()

if(RIVEQT_ENABLE_VULKAN)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rivevulkanbridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rivevulkanbridge.h
    )
endif()

if(APPLE AND RIVEQT_ENABLE_METAL)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rivemetalbridge.mm
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rivemetalbridge.h
    )
    set_source_files_properties(
        private/rivemetalbridge.mm
        PROPERTIES
            COMPILE_OPTIONS "-fobjc-arc"
    )
endif()

set(RIVEQT_QML_MODULE_ARGS)
if(IOS)
    list(APPEND RIVEQT_QML_MODULE_ARGS STATIC)
endif()

qt_add_qml_module(RiveQtQuick
    ${RIVEQT_QML_MODULE_ARGS}
    URI RiveQtQuick
    VERSION 2.0
    PLUGIN_TARGET RiveQtQuickPlugin
    SOURCES
        ${RIVEQTQUICK_SOURCES}
        ${RIVEQTQUICK_HEADERS}
)

target_include_directories(RiveQtQuick
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(RiveQtQuick
    PRIVATE
        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Qml
        Qt6::Quick
        Rive::Official
)

target_compile_definitions(RiveQtQuick
    PRIVATE
        $<$<BOOL:${RIVEQT_ENABLE_D3D12}>:RIVEQT_ENABLE_D3D12>
        $<$<BOOL:${RIVEQT_ENABLE_VULKAN}>:RIVEQT_ENABLE_VULKAN>
)
if(APPLE AND RIVEQT_ENABLE_METAL)
    target_compile_definitions(RiveQtQuick PRIVATE RIVEQT_ENABLE_METAL)
endif()

install(TARGETS RiveQtQuick RiveQtQuickPlugin
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
