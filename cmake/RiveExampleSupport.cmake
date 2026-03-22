function(_riveqt_is_ios_simulator_build out_var)
    set(result OFF)
    if(IOS)
        string(TOLOWER "${CMAKE_OSX_SYSROOT}" riveqt_osx_sysroot_lower)
        if(riveqt_osx_sysroot_lower MATCHES "iphonesimulator")
            set(result ON)
        endif()
    endif()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(_riveqt_create_qt_plugin_import_source imported_target plugin_target out_var)
    get_target_property(plugin_class_name "${plugin_target}" QT_PLUGIN_CLASS_NAME)
    if(NOT plugin_class_name OR plugin_class_name STREQUAL "plugin_class_name-NOTFOUND")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    string(MAKE_C_IDENTIFIER "${imported_target}" sanitized_target)
    set(source_file
        "${CMAKE_CURRENT_BINARY_DIR}/qt-ios-import-stubs/${sanitized_target}_plugin_init.cpp")
    file(GENERATE
        OUTPUT "${source_file}"
        CONTENT
            "#include <QtPlugin>\nQ_IMPORT_PLUGIN(${plugin_class_name})\n"
    )
    set(${out_var} "${source_file}" PARENT_SCOPE)
endfunction()

function(_riveqt_create_qt_resource_import_source imported_target object_path out_var)
    get_filename_component(object_name "${object_path}" NAME)
    if(NOT object_name MATCHES "^qrc_(.+)_init\\.cpp\\.o$")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    set(resource_name "${CMAKE_MATCH_1}")
    string(MAKE_C_IDENTIFIER "${imported_target}_${resource_name}" sanitized_target)
    set(source_file
        "${CMAKE_CURRENT_BINARY_DIR}/qt-ios-import-stubs/${sanitized_target}_resource_init.cpp")
    file(GENERATE
        OUTPUT "${source_file}"
        CONTENT
            "#include <QtCore/qtsymbolmacros.h>\n\nQT_DECLARE_EXTERN_RESOURCE(${resource_name})\n\nnamespace {\nstruct resourceReferenceKeeper {\n    resourceReferenceKeeper() { QT_KEEP_RESOURCE(${resource_name}) }\n} resourceReferenceKeeperInstance;\n}\n"
    )
    set(${out_var} "${source_file}" PARENT_SCOPE)
endfunction()

function(_riveqt_create_qt_qml_plugin_source imported_target out_var)
    get_target_property(plugin_class_name "${imported_target}" QT_PLUGIN_CLASS_NAME)
    if(NOT plugin_class_name OR plugin_class_name STREQUAL "plugin_class_name-NOTFOUND")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    string(MAKE_C_IDENTIFIER "${imported_target}" sanitized_target)
    set(source_file
        "${CMAKE_CURRENT_BINARY_DIR}/qt-ios-import-stubs/${sanitized_target}_qml_plugin.cpp")
    file(GENERATE
        OUTPUT "${source_file}"
        CONTENT
            "#include <QtQml/qqmlextensionplugin.h>\n\nclass ${plugin_class_name} : public QQmlEngineExtensionPlugin\n{\n    Q_OBJECT\n    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)\n\npublic:\n    explicit ${plugin_class_name}(QObject* parent = nullptr)\n        : QQmlEngineExtensionPlugin(parent)\n    {\n    }\n};\n\n#include \"${sanitized_target}_qml_plugin.moc\"\n"
    )
    set(${out_var} "${source_file}" PARENT_SCOPE)
endfunction()

function(_riveqt_get_replacement_library_reference replacement_target out_var)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set(reference
            "${CMAKE_BINARY_DIR}/\$(CONFIGURATION)\$(EFFECTIVE_PLATFORM_NAME)/"
            "${CMAKE_STATIC_LIBRARY_PREFIX}${replacement_target}${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
        string(REPLACE ";" "" reference "${reference}")
    else()
        set(reference "$<TARGET_FILE:${replacement_target}>")
    endif()

    set(${out_var} "${reference}" PARENT_SCOPE)
endfunction()

function(_riveqt_get_ios_simulator_qml_plugin_replacement_targets out_var)
    get_property(replacement_targets GLOBAL PROPERTY TARGETS)
    if(replacement_targets)
        list(FILTER replacement_targets INCLUDE REGEX "^riveqt_ios_qml_plugin_")
    else()
        set(replacement_targets "")
    endif()
    set(${out_var} "${replacement_targets}" PARENT_SCOPE)
endfunction()

function(_riveqt_ios_simulator_requires_qml_plugin_replacement out_var)
    _riveqt_is_ios_simulator_build(is_ios_simulator)
    if(NOT is_ios_simulator)
        set(${out_var} OFF PARENT_SCOPE)
        return()
    endif()

    set(result OFF)
    set(simulator_archs ${CMAKE_OSX_ARCHITECTURES})
    if(simulator_archs)
        list(FIND simulator_archs "arm64" arm64_index)
        if(arm64_index GREATER -1)
            set(result ON)
        endif()
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(result ON)
    endif()

    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(_riveqt_add_ios_simulator_qml_plugin_dependencies consumer_target)
    _riveqt_get_ios_simulator_qml_plugin_replacement_targets(replacement_targets)
    if(replacement_targets)
        add_dependencies("${consumer_target}" ${replacement_targets})
        target_link_libraries("${consumer_target}"
            PRIVATE
                ${replacement_targets}
        )
    endif()
endfunction()

function(_riveqt_register_ios_simulator_qml_plugin_consumer consumer_target)
    get_property(registered_consumers
        GLOBAL PROPERTY _riveqt_ios_simulator_qml_plugin_consumers)
    if(NOT registered_consumers)
        set(registered_consumers "")
    endif()

    if(NOT "${consumer_target}" IN_LIST registered_consumers)
        list(APPEND registered_consumers "${consumer_target}")
        set_property(GLOBAL PROPERTY
            _riveqt_ios_simulator_qml_plugin_consumers
            "${registered_consumers}"
        )
    endif()

    _riveqt_add_ios_simulator_qml_plugin_dependencies("${consumer_target}")
endfunction()

function(_riveqt_add_replacement_qml_plugin_dependency_to_registered_consumers replacement_target)
    get_property(registered_consumers
        GLOBAL PROPERTY _riveqt_ios_simulator_qml_plugin_consumers)
    foreach(consumer_target IN LISTS registered_consumers)
        if(TARGET "${consumer_target}")
            add_dependencies("${consumer_target}" "${replacement_target}")
            target_link_libraries("${consumer_target}"
                PRIVATE
                    "${replacement_target}"
            )
        endif()
    endforeach()
endfunction()

function(_riveqt_apply_ios_simulator_replacement_mappings_to_target consumer_target)
    get_property(replacement_mappings GLOBAL PROPERTY _riveqt_ios_simulator_replacement_mappings)
    if(NOT replacement_mappings)
        return()
    endif()

    foreach(target_property
        IN ITEMS
            INTERFACE_LINK_LIBRARIES
            INTERFACE_LINK_OPTIONS
            INTERFACE_SOURCES
            LINK_LIBRARIES
            LINK_OPTIONS
            SOURCES
    )
        get_target_property(property_value "${consumer_target}" "${target_property}")
        if(NOT property_value OR property_value STREQUAL "property_value-NOTFOUND")
            continue()
        endif()

        set(updated_value "")
        foreach(property_item IN LISTS property_value)
            set(updated_item "${property_item}")
            foreach(replacement_mapping IN LISTS replacement_mappings)
                string(REPLACE "|" ";" replacement_parts "${replacement_mapping}")
                list(GET replacement_parts 0 original_reference)
                list(GET replacement_parts 1 replacement_reference)
                list(LENGTH replacement_parts replacement_parts_count)
                if(replacement_parts_count GREATER 2)
                    list(GET replacement_parts 2 replacement_kind)
                else()
                    set(replacement_kind "target")
                endif()

                if(updated_item STREQUAL "${original_reference}")
                    set(updated_item "${replacement_reference}")
                elseif(updated_item STREQUAL "$<LINK_ONLY:${original_reference}>")
                    set(updated_item "$<LINK_ONLY:${replacement_reference}>")
                endif()

                if(replacement_kind STREQUAL "target"
                   AND TARGET "${replacement_reference}")
                    get_target_property(replacement_type "${replacement_reference}" TYPE)
                else()
                    set(replacement_type "")
                endif()

                if(replacement_kind STREQUAL "target")
                    string(REPLACE
                        ":${original_reference}>"
                        ":${replacement_reference}>"
                        updated_item
                        "${updated_item}"
                    )
                else()
                    string(REPLACE
                        "${original_reference}"
                        "${replacement_reference}"
                        updated_item
                        "${updated_item}"
                    )
                endif()

                if(replacement_kind STREQUAL "target"
                   AND replacement_type STREQUAL "OBJECT_LIBRARY")
                    string(REPLACE
                        "$<TARGET_OBJECTS:${original_reference}>"
                        "$<TARGET_OBJECTS:${replacement_reference}>"
                        updated_item
                        "${updated_item}"
                    )
                endif()
            endforeach()
            list(APPEND updated_value "${updated_item}")
        endforeach()

        if(NOT "${updated_value}" STREQUAL "${property_value}")
            set_property(TARGET "${consumer_target}" PROPERTY
                "${target_property}" "${updated_value}"
            )
        endif()
    endforeach()
endfunction()

function(_riveqt_apply_ios_simulator_replacement_mappings_to_all_targets)
    get_property(replacement_mappings GLOBAL PROPERTY _riveqt_ios_simulator_replacement_mappings)
    if(NOT replacement_mappings)
        return()
    endif()

    get_property(all_build_targets GLOBAL PROPERTY TARGETS)
    get_property(imported_targets GLOBAL PROPERTY _riveqt_ios_simulator_imported_targets)
    list(APPEND all_build_targets ${imported_targets})
    list(REMOVE_DUPLICATES all_build_targets)

    foreach(consumer_target IN LISTS all_build_targets)
        _riveqt_apply_ios_simulator_replacement_mappings_to_target("${consumer_target}")
    endforeach()
endfunction()

function(_riveqt_patch_qt_ios_simulator_imported_objects)
    _riveqt_is_ios_simulator_build(is_ios_simulator)
    if(NOT is_ios_simulator)
        return()
    endif()

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt-ios-import-stubs")

    get_property(imported_targets DIRECTORY PROPERTY IMPORTED_TARGETS)
    set_property(GLOBAL PROPERTY _riveqt_ios_simulator_imported_targets "${imported_targets}")
    set(replacement_mappings "")
    foreach(target IN LISTS imported_targets)
        get_target_property(is_imported "${target}" IMPORTED)
        if(NOT is_imported)
            continue()
        endif()

        get_target_property(already_patched "${target}" _riveqt_ios_simulator_stub_patched)
        if(already_patched)
            continue()
        endif()

        get_target_property(imported_configs "${target}" IMPORTED_CONFIGURATIONS)
        if(NOT imported_configs OR imported_configs STREQUAL "imported_configs-NOTFOUND")
            set(imported_configs DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        endif()

        set(imported_objects "")
        foreach(config IN LISTS imported_configs)
            get_target_property(config_objects "${target}" "IMPORTED_OBJECTS_${config}")
            if(config_objects AND NOT config_objects STREQUAL "config_objects-NOTFOUND")
                list(APPEND imported_objects ${config_objects})
            endif()
        endforeach()

        get_target_property(generic_objects "${target}" IMPORTED_OBJECTS)
        if(generic_objects AND NOT generic_objects STREQUAL "generic_objects-NOTFOUND")
            list(APPEND imported_objects ${generic_objects})
        endif()

        list(REMOVE_DUPLICATES imported_objects)
        if(NOT imported_objects)
            get_target_property(plugin_type "${target}" QT_PLUGIN_TYPE)
            get_target_property(is_qml_plugin_target "${target}" _qt_qml_module_is_plugin_target)
            if(NOT plugin_type STREQUAL "qml_plugin"
               AND NOT is_qml_plugin_target)
                continue()
            endif()
        endif()

        set(generated_sources "")

        if(target MATCHES "_init$")
            string(REGEX REPLACE "_init$" "" plugin_target "${target}")
            if(TARGET "${plugin_target}")
                _riveqt_create_qt_plugin_import_source("${target}" "${plugin_target}" plugin_source)
                if(plugin_source)
                    list(APPEND generated_sources "${plugin_source}")
                endif()
            endif()
        endif()

        set(seen_resource_names "")
        foreach(object_path IN LISTS imported_objects)
            get_filename_component(object_name "${object_path}" NAME)
            if(object_name MATCHES "^qrc_(.+)_init\\.cpp\\.o$")
                set(resource_name "${CMAKE_MATCH_1}")
                if(resource_name IN_LIST seen_resource_names)
                    continue()
                endif()
                list(APPEND seen_resource_names "${resource_name}")
            endif()

            _riveqt_create_qt_resource_import_source("${target}" "${object_path}" resource_source)
            if(resource_source)
                list(APPEND generated_sources "${resource_source}")
            endif()
        endforeach()

        if(generated_sources)
            string(MAKE_C_IDENTIFIER "${target}" sanitized_target)
            set(replacement_target "riveqt_ios_stub_${sanitized_target}")
            if(NOT TARGET "${replacement_target}")
                add_library("${replacement_target}" OBJECT ${generated_sources})
                target_link_libraries("${replacement_target}"
                    PRIVATE
                        Qt6::Core
                )

                get_target_property(interface_link_libraries "${target}" INTERFACE_LINK_LIBRARIES)
                if(interface_link_libraries AND NOT interface_link_libraries STREQUAL "interface_link_libraries-NOTFOUND")
                    target_link_libraries("${replacement_target}"
                        INTERFACE
                            ${interface_link_libraries}
                    )
                endif()

                get_target_property(is_qt_plugin_init_target "${target}" _is_qt_plugin_init_target)
                if(is_qt_plugin_init_target)
                    set_property(TARGET "${replacement_target}" PROPERTY
                        _is_qt_plugin_init_target "${is_qt_plugin_init_target}"
                    )
                endif()

                set_property(TARGET "${replacement_target}" PROPERTY _is_qt_propagated_object_library TRUE)
            endif()

            # Keep the original imported target around for any unresolved references, but strip out
            # its prebuilt object payload and forward consumers to the locally generated
            # simulator-safe replacement instead.
            get_target_property(original_interface_link_libraries "${target}" INTERFACE_LINK_LIBRARIES)
            if(original_interface_link_libraries AND NOT original_interface_link_libraries STREQUAL "original_interface_link_libraries-NOTFOUND")
                set_property(TARGET "${target}" PROPERTY
                    INTERFACE_LINK_LIBRARIES
                    "${original_interface_link_libraries};${replacement_target}"
                )
            else()
                set_property(TARGET "${target}" PROPERTY
                    INTERFACE_LINK_LIBRARIES
                    "${replacement_target}"
                )
            endif()

            foreach(config IN LISTS imported_configs)
                set_property(TARGET "${target}" PROPERTY "IMPORTED_OBJECTS_${config}" "")
            endforeach()
            set_property(TARGET "${target}" PROPERTY IMPORTED_OBJECTS "")

            list(APPEND replacement_mappings "${target}|${replacement_target}|target")
            set_property(TARGET "${target}" PROPERTY _riveqt_ios_simulator_stub_patched TRUE)
        endif()

        get_target_property(plugin_type "${target}" QT_PLUGIN_TYPE)
        get_target_property(is_qml_plugin_target "${target}" _qt_qml_module_is_plugin_target)
        if(NOT plugin_type STREQUAL "qml_plugin"
           AND NOT is_qml_plugin_target)
            continue()
        endif()

        _riveqt_ios_simulator_requires_qml_plugin_replacement(
            replace_qml_plugin_target)
        if(NOT replace_qml_plugin_target)
            continue()
        endif()

        get_target_property(already_patched_qml_plugin "${target}" _riveqt_ios_simulator_qml_plugin_patched)
        if(already_patched_qml_plugin)
            continue()
        endif()

        _riveqt_create_qt_qml_plugin_source("${target}" plugin_source)
        if(NOT plugin_source)
            continue()
        endif()

        string(MAKE_C_IDENTIFIER "${target}" sanitized_target)
        set(replacement_target "riveqt_ios_qml_plugin_${sanitized_target}")
        if(NOT TARGET "${replacement_target}")
            add_library("${replacement_target}" STATIC "${plugin_source}")
            set_property(TARGET "${replacement_target}" PROPERTY AUTOMOC ON)
            target_link_libraries("${replacement_target}"
                PRIVATE
                    Qt6::Core
                    Qt6::Qml
            )

            foreach(interface_property
                IN ITEMS
                    INTERFACE_COMPILE_DEFINITIONS
                    INTERFACE_COMPILE_OPTIONS
                    INTERFACE_INCLUDE_DIRECTORIES
                    INTERFACE_LINK_DIRECTORIES
                    INTERFACE_LINK_LIBRARIES
                    INTERFACE_LINK_OPTIONS
                    INTERFACE_SOURCES
            )
                get_target_property(interface_value "${target}" "${interface_property}")
                if(interface_value AND NOT interface_value STREQUAL "interface_value-NOTFOUND")
                    set_property(TARGET "${replacement_target}" PROPERTY
                        "${interface_property}" "${interface_value}"
                    )
                endif()
            endforeach()
        endif()

        get_target_property(original_qml_interface_link_libraries
            "${target}" INTERFACE_LINK_LIBRARIES)
        if(original_qml_interface_link_libraries
           AND NOT original_qml_interface_link_libraries STREQUAL
                   "original_qml_interface_link_libraries-NOTFOUND")
            if(NOT "${replacement_target}" IN_LIST original_qml_interface_link_libraries)
                set_property(TARGET "${target}" PROPERTY
                    INTERFACE_LINK_LIBRARIES
                    "${original_qml_interface_link_libraries};${replacement_target}"
                )
            endif()
        else()
            set_property(TARGET "${target}" PROPERTY
                INTERFACE_LINK_LIBRARIES
                "${replacement_target}"
            )
        endif()

        _riveqt_get_replacement_library_reference("${replacement_target}" replacement_library_reference)
        _riveqt_add_replacement_qml_plugin_dependency_to_registered_consumers(
            "${replacement_target}")
        set_property(TARGET "${target}" PROPERTY
            IMPORTED_LOCATION
            "${replacement_library_reference}"
        )
        foreach(config IN LISTS imported_configs)
            set_property(TARGET "${target}" PROPERTY
                "IMPORTED_LOCATION_${config}"
                "${replacement_library_reference}"
            )
        endforeach()

        list(APPEND replacement_mappings "${target}|${replacement_target}|target")
        list(APPEND replacement_mappings
            "$<TARGET_FILE:${replacement_target}>|${replacement_library_reference}|item")
        list(APPEND replacement_mappings
            "$<LINK_ONLY:$<TARGET_FILE:${replacement_target}>>|${replacement_library_reference}|item")
        foreach(imported_location_property IN ITEMS IMPORTED_LOCATION IMPORTED_IMPLIB)
            get_target_property(imported_location "${target}" "${imported_location_property}")
            if(imported_location AND NOT imported_location STREQUAL "imported_location-NOTFOUND")
                list(APPEND replacement_mappings
                    "${imported_location}|${replacement_library_reference}|item")
            endif()

            foreach(config IN LISTS imported_configs)
                get_target_property(imported_location
                    "${target}"
                    "${imported_location_property}_${config}")
                if(imported_location AND NOT imported_location STREQUAL "imported_location-NOTFOUND")
                    list(APPEND replacement_mappings
                        "${imported_location}|${replacement_library_reference}|item")
                endif()
            endforeach()
        endforeach()
        set_property(TARGET "${target}" PROPERTY _riveqt_ios_simulator_qml_plugin_patched TRUE)
    endforeach()

    if(NOT replacement_mappings)
        return()
    endif()

    set_property(GLOBAL PROPERTY
        _riveqt_ios_simulator_replacement_mappings "${replacement_mappings}")

    _riveqt_apply_ios_simulator_replacement_mappings_to_all_targets()
endfunction()

function(riveqt_configure_ios_bundle target bundle_suffix)
    if(NOT IOS)
        return()
    endif()

    string(TOLOWER "${bundle_suffix}" normalized_suffix)
    string(REGEX REPLACE "[^a-z0-9.-]" "-" normalized_suffix "${normalized_suffix}")
    set(bundle_id "${RIVEQT_IOS_BUNDLE_ID_PREFIX}.${normalized_suffix}")

    set_target_properties(${target}
        PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "${target}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${bundle_id}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${bundle_id}"
    )

    if(RIVEQT_IOS_DEVELOPMENT_TEAM)
        set_target_properties(${target}
            PROPERTIES
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${RIVEQT_IOS_DEVELOPMENT_TEAM}"
        )
    endif()
endfunction()

_riveqt_patch_qt_ios_simulator_imported_objects()

function(riveqt_add_example_app_support target)
    set(oneValueArgs QML_FILE IOS_MODULE_URI BUNDLE_SUFFIX QMLDIR)
    set(multiValueArgs RESOURCE_FILES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_QML_FILE)
        message(FATAL_ERROR "riveqt_add_example_app_support requires QML_FILE.")
    endif()
    if(NOT ARG_QMLDIR)
        message(FATAL_ERROR "riveqt_add_example_app_support requires QMLDIR.")
    endif()

    if(IOS)
        if(NOT ARG_IOS_MODULE_URI)
            message(FATAL_ERROR "riveqt_add_example_app_support requires IOS_MODULE_URI for iOS builds.")
        endif()
        if(NOT ARG_BUNDLE_SUFFIX)
            message(FATAL_ERROR "riveqt_add_example_app_support requires BUNDLE_SUFFIX for iOS builds.")
        endif()

        # Qt's iOS package currently ships several imported *_init object stubs whose arm64
        # slice is built for device iOS rather than the simulator. We disable the late static
        # plugin finalizer path here so the example targets use the simulator-safe replacement
        # stubs wired up above instead of the original imported objects.
        set_property(TARGET ${target} PROPERTY _qt_static_plugins_finalizer_mode FALSE)
        if(TARGET RiveQtQuickPlugin)
            target_link_libraries(${target}
                PRIVATE
                    RiveQtQuickPlugin
            )
        endif()

        get_filename_component(qml_file_name "${ARG_QML_FILE}" NAME)
        set_source_files_properties("${ARG_QML_FILE}"
            PROPERTIES
                QT_RESOURCE_ALIAS "${qml_file_name}"
        )
        qt_add_qml_module(${target}
            URI "${ARG_IOS_MODULE_URI}"
            VERSION 1.0
            QML_FILES
                "${ARG_QML_FILE}"
        )

        if(ARG_RESOURCE_FILES)
            qt_add_resources(${target} "${target}_bundled_assets"
                PREFIX "/"
                BASE "${CMAKE_SOURCE_DIR}"
                FILES
                    ${ARG_RESOURCE_FILES}
            )
        endif()

        qt_import_qml_plugins(${target})
        _riveqt_register_ios_simulator_qml_plugin_consumer("${target}")
        _riveqt_patch_qt_ios_simulator_imported_objects()
        _riveqt_apply_ios_simulator_replacement_mappings_to_all_targets()
        _riveqt_add_ios_simulator_qml_plugin_dependencies("${target}")
        riveqt_configure_ios_bundle(${target} "${ARG_BUNDLE_SUFFIX}")
        return()
    endif()

    get_filename_component(qml_file_name "${ARG_QML_FILE}" NAME)
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ARG_QML_FILE}"
            "$<TARGET_FILE_DIR:${target}>/${qml_file_name}"
        COMMENT "Copying ${target} Main.qml"
    )

    riveqt_add_windeployqt(${target} QMLDIR "${ARG_QMLDIR}")
    riveqt_deploy_local_qml_module(${target} RiveQtQuick "${CMAKE_BINARY_DIR}/src/RiveQtQuick" RiveQtQuickPlugin)
    riveqt_deploy_runtime_dll(${target} RiveQtQuick)
endfunction()
