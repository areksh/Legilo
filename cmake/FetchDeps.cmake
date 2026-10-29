# Downloads the archives listed in Dependencies.cmake into
# third_party/downloads/, checking each against its SHA-256. Archives that
# are already present and intact are left alone.
#
# Usage: cmake -P cmake/FetchDeps.cmake   (or scripts/fetch-deps)

cmake_minimum_required(VERSION 3.25)

include(${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake)

get_filename_component(downloads "${CMAKE_CURRENT_LIST_DIR}/../third_party/downloads" ABSOLUTE)
file(MAKE_DIRECTORY "${downloads}")

foreach(entry IN LISTS LEGILO_DEPENDENCIES)
    legilo_dependency_fields("${entry}" name url hash)
    set(file "${downloads}/${name}")

    if(EXISTS "${file}")
        file(SHA256 "${file}" actual)
        if(actual STREQUAL hash)
            message(STATUS "Have ${name}")
            continue()
        endif()
        message(STATUS "Replacing ${name}: its SHA-256 does not match")
    endif()

    # Download beside the target and rename, so an interrupted download
    # never leaves a partial archive under the expected name.
    message(STATUS "Downloading ${name}")
    file(DOWNLOAD "${url}" "${file}.part" EXPECTED_HASH SHA256=${hash} STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${file}.part")
        list(GET status 1 reason)
        message(FATAL_ERROR "Could not download ${url}: ${reason}")
    endif()
    file(RENAME "${file}.part" "${file}")
endforeach()
