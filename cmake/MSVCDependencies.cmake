function(_minvex_configure_msvc_dependencies)
    set(probe_directory "${CMAKE_BINARY_DIR}/CMakeFiles/MinvexShowIncludes")
    file(MAKE_DIRECTORY "${probe_directory}")
    file(WRITE "${probe_directory}/probe.h" "\n")
    file(WRITE "${probe_directory}/probe.cpp" "#include \"probe.h\"\n")
    # CMake takes UTF-8 and encodes the prefix for Ninja. MSVC can already
    # emit UTF-8 even if AUTO would pick the console input code page.
    foreach(probe_encoding IN ITEMS NONE AUTO)
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c /TP probe.cpp /Foprobe.obj
            WORKING_DIRECTORY "${probe_directory}"
            OUTPUT_VARIABLE probe_output
            ERROR_VARIABLE probe_error
            RESULT_VARIABLE probe_result
            ENCODING ${probe_encoding}
        )
        string(REGEX MATCH "[^\r\n]*MinvexShowIncludes[/\\\\]probe\\.h" probe_line "${probe_output}")
        string(REGEX REPLACE "[A-Za-z]:[/\\\\].*$" "" probe_prefix "${probe_line}")
        if(NOT probe_result STREQUAL "0" OR NOT probe_prefix)
            message(FATAL_ERROR "Could not detect the MSVC include prefix for Ninja: ${probe_output}${probe_error}")
        endif()

        # Round-trip the prefix through CMake's UTF-8 reader before giving it
        # to the generator. Non-UTF-8 bytes need decoding in headless builds.
        file(WRITE "${probe_directory}/prefix.txt" "${probe_prefix}\n")
        file(STRINGS "${probe_directory}/prefix.txt" utf8_prefix ENCODING UTF-8)
        if("${probe_prefix}" STREQUAL "${utf8_prefix}")
            set(CMAKE_CL_SHOWINCLUDES_PREFIX "${probe_prefix}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Could not decode the MSVC /showIncludes prefix as UTF-8. Configure and build with the same MSVC language environment.")
endfunction()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_GENERATOR MATCHES "^Ninja")
    _minvex_configure_msvc_dependencies()
endif()
