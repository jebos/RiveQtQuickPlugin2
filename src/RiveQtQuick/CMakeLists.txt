set(RIVEQTQUICK_SOURCES
    rivebinding.cpp
    riveitem.cpp
    riveqtquickplugin.cpp
    private/rivelogging.cpp
    private/rivefileassetloader.cpp
    private/riveinspector.cpp
    private/rendering/rivebackendbridge.cpp
    private/rendering/riverendernode.cpp
)

set(RIVEQTQUICK_HEADERS
    rivebinding.h
    riveitem.h
    private/rivelogging.h
    private/rivefileassetloader.h
    private/riveinspector.h
    private/rendering/rivebackendbridge.h
    private/rendering/riverendernode.h
    private/rendering/riverenderstate.h
)

if(RIVEQT_ENABLE_OPENGL)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/qtglprocaddress.cpp
        private/rendering/riveglbridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/qtglprocaddress.h
        private/rendering/riveglbridge.h
    )
endif()

if(RIVEQT_ENABLE_SOFTWARE)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/rivesoftwarebridge.cpp
        private/rendering/qpainter/qpainterrivefactory.cpp
        private/rendering/qpainter/qpainterriveimage.cpp
        private/rendering/qpainter/qpainterrivepaint.cpp
        private/rendering/qpainter/qpainterrivepath.cpp
        private/rendering/qpainter/qpainterriverenderer.cpp
        private/rendering/qpainter/qpainterriveshader.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/rivesoftwarebridge.h
        private/rendering/qpainter/qpainterrivefactory.h
        private/rendering/qpainter/qpainterriveimage.h
        private/rendering/qpainter/qpainterrivepaint.h
        private/rendering/qpainter/qpainterrivepath.h
        private/rendering/qpainter/qpainterriverenderer.h
        private/rendering/qpainter/qpainterriveshader.h
    )
endif()

if(WIN32)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/rived3d11bridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/rived3d11bridge.h
    )
endif()

if(RIVEQT_ENABLE_D3D12)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/rived3d12bridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/rived3d12bridge.h
    )
endif()

if(RIVEQT_ENABLE_VULKAN)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/rivevulkanbridge.cpp
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/rivevulkanbridge.h
    )
endif()

if(APPLE AND RIVEQT_ENABLE_METAL)
    list(APPEND RIVEQTQUICK_SOURCES
        private/rendering/rivemetalbridge.mm
    )
    list(APPEND RIVEQTQUICK_HEADERS
        private/rendering/rivemetalbridge.h
    )
    set_source_files_properties(
        private/rendering/rivemetalbridge.mm
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
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/private
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
        $<$<BOOL:${RIVEQT_ENABLE_OPENGL}>:RIVEQT_ENABLE_OPENGL>
        $<$<BOOL:${RIVEQT_ENABLE_SOFTWARE}>:RIVEQT_ENABLE_SOFTWARE>
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
