if(NOT DEFINED RUNTIME_CONFIGURATION OR NOT DEFINED PACKAGE_DIRECTORY)
    message(FATAL_ERROR "Runtime configuration and package directory are required")
endif()
include("${RUNTIME_CONFIGURATION}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${RUNTIME_DEPENDENCY_COMMAND}")
get_filename_component(qt_runtime_directory "${QT_CORE_FILE}" DIRECTORY)
file(GLOB_RECURSE runtime_libraries "${PACKAGE_DIRECTORY}/*.dll")
# Scan plugin imports as well as both executables: windeployqt alone does not
# include every shared dependency of all Qt distributions.
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${PACKAGE_DIRECTORY}/OTEditor.exe" "${PACKAGE_DIRECTORY}/OTEditorUpdater.exe"
    LIBRARIES ${runtime_libraries}
    DIRECTORIES "${PACKAGE_DIRECTORY}" "${qt_runtime_directory}"
    PRE_EXCLUDE_REGEXES "^(api-ms-|ext-ms-)"
    POST_EXCLUDE_REGEXES ".*[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
    RESOLVED_DEPENDENCIES_VAR resolved_libraries
    UNRESOLVED_DEPENDENCIES_VAR unresolved_libraries)
if(unresolved_libraries)
    message(FATAL_ERROR "Unresolved release dependencies: ${unresolved_libraries}")
endif()
foreach(runtime_library IN LISTS resolved_libraries)
    get_filename_component(runtime_name "${runtime_library}" NAME)
    if(NOT EXISTS "${PACKAGE_DIRECTORY}/${runtime_name}")
        file(COPY "${runtime_library}" DESTINATION "${PACKAGE_DIRECTORY}")
    endif()
endforeach()
file(GLOB runtime_notices "${qt_runtime_directory}/../share/*/copyright")
foreach(runtime_notice IN LISTS runtime_notices)
    get_filename_component(notice_directory "${runtime_notice}" DIRECTORY)
    get_filename_component(dependency_name "${notice_directory}" NAME)
    file(COPY "${runtime_notice}" DESTINATION "${PACKAGE_DIRECTORY}/licenses/${dependency_name}")
endforeach()
