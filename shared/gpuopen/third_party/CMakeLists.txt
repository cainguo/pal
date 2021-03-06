cmake_minimum_required(VERSION 3.5)
project(DevDriverThirdParty CXX)


# We cannot fix warnings in third-party code, so we silence them instead.
# This should never be used on code you can easily modify.
function(dd_third_party_disable_warnings name)

    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

        # [GCC] Options to Request or Suppress Warnings
        #   https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
        target_compile_options(${name}
            PRIVATE
                -w
        )

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

        # [MSVC] Warning Level
        #   https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level
        target_compile_options(${name}
            PRIVATE
                /w
        )

    else()

        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")

    endif()

endfunction()

# dd_third_party_add_lib: Add a third party library.
#   This function is a no-op if the target in question already exists,
#   otherwise it creates it by calling `add_subdirectory()` on the directory specified with DIRECTORY, or by TARGET.
#
#   Named Arguments:
#       [TARGET target]       Name of target that should be created. Causes a fatal error when the target isn't created
#       [DIRECTORY directory] Directory to pass to `add_subdirectory()`. If omitted, defaults to value of TARGET
#
#   Basic Usage:
#       dd_third_party_add_lib(TARGET mpack)
#
#   Advanced Usage:
#       dd_third_party_add_lib(TARGET mpack DIRECTORY git-submodules/mpack)
#
function(dd_third_party_add_lib)

    ## This parses the arguments into the format we expect.
    ## For comprehensive documentation, see the CMake Docs:
    ## https://cmake.org/cmake/help/v3.10/command/cmake_parse_arguments.html#command:cmake_parse_arguments
    # 'options' is a list of things that are used or not - like `--flags` in CLI apps.
    set(options         "")
    # 'multiValueArgs' is a list of things that have one or more arguments.
    set(multiValueArgs  "")
    # 'oneValueArgs' is a list of things that expect exactly one argument, like `-j12`, `-j=12`, or `-j 12`.
    set(oneValueArgs    TARGET DIRECTORY)

    cmake_parse_arguments(third_party "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (third_party_UNPARSED_ARGUMENTS)

        message(FATAL_ERROR "Unexpected arguments: ${third_party_UNPARSED_ARGUMENTS}")

    endif()

    if (third_party_KEYWORDS_MISSING_VALUES)

        message(FATAL_ERROR "The following arguments are missing their values: ${third_party_KEYWORDS_MISSING_VALUES}")

    endif()

    if (NOT third_party_DIRECTORY)

        set(third_party_DIRECTORY ${third_party_TARGET})

    endif()

    message(STATUS "Adding third party target \"${third_party_TARGET}\" from ${CMAKE_CURRENT_SOURCE_DIR}/${third_party_DIRECTORY}")
    add_subdirectory("${third_party_DIRECTORY}" EXCLUDE_FROM_ALL)

    if (TARGET ${third_party_TARGET})

        amd_target_options(${third_party_TARGET})
        dd_third_party_disable_warnings(${third_party_TARGET})

        # Only libraries need PIC enabled
        get_target_property(target_type ${third_party_TARGET} TYPE)
        if (target_type STREQUAL "STATIC_LIBRARY")

            set_target_properties(${third_party_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)

        endif()

        # INTERFACE_LIBRARY targets have restricted properties, so we skip them.
        if (NOT target_type STREQUAL "INTERFACE_LIBRARY")

            set_target_properties(${third_party_TARGET}
                PROPERTIES
                    FOLDER              "Third Party"
            )

        endif()

    else()

        message(FATAL_ERROR "Failed to add third party target ${third_party_TARGET}")

    endif()

endfunction()

### MessagePack Reader/Writer ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET mpack)

    dd_third_party_add_lib(TARGET mpack)

endif()

### JSON Reader/Writer ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET rapidjson)

    dd_third_party_add_lib(TARGET rapidjson)

endif()

### Fast Hasher ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET metrohash)

    dd_third_party_add_lib(TARGET metrohash)

endif()

### Fast (De)Compression ###
# WA: Until we have a global CMake context, we need to check that this target hasn't been added already.
if (NOT TARGET lz4)

    dd_third_party_add_lib(TARGET lz4)

endif()

### C++ Unit Tests ###
if (NOT TARGET gtest)

    dd_third_party_add_lib(TARGET gtest)

endif()
