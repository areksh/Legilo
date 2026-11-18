# Downloads the archives listed in Dependencies.cmake into
# third_party/downloads/, checking each against its SHA-256. Archives that
# are already present and intact are left alone.
#
# With -DLEGILO_TEST_VOICES=ON it also fetches the test voices and unpacks
# each into third_party/test-voices/, skipping any already unpacked from
# the same archive.
#
# Usage: cmake [-DLEGILO_TEST_VOICES=ON] -P cmake/FetchDeps.cmake
#        (or scripts/fetch-deps [--test-voices])

cmake_minimum_required(VERSION 3.25)

include(${CMAKE_CURRENT_LIST_DIR}/Dependencies.cmake)

get_filename_component(third_party "${CMAKE_CURRENT_LIST_DIR}/../third_party" ABSOLUTE)

# legilo_download(<url> <file> <sha256>)
# Downloads beside the target and renames, so an interrupted download never
# leaves a partial archive under the expected name.
function(legilo_download url file hash)
    get_filename_component(name "${file}" NAME)
    message(STATUS "Downloading ${name}")
    file(DOWNLOAD "${url}" "${file}.part" EXPECTED_HASH SHA256=${hash} STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${file}.part")
        list(GET status 1 reason)
        message(FATAL_ERROR "Could not download ${url}: ${reason}")
    endif()
    file(RENAME "${file}.part" "${file}")
endfunction()

set(downloads "${third_party}/downloads")
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
    legilo_download("${url}" "${file}" "${hash}")
endforeach()

# Each word list package is unpacked to its list, under its usual name
# (british-english, american-english). A stamp beside the list records the
# package's hash, so a changed pin unpacks afresh.
set(word_lists "${third_party}/word-lists")
file(MAKE_DIRECTORY "${word_lists}")
foreach(entry IN LISTS LEGILO_WORD_LISTS)
    legilo_dependency_fields("${entry}" name url hash)
    set(stamp "${word_lists}/.${name}.sha256")
    if(EXISTS "${stamp}")
        file(READ "${stamp}" unpacked)
        if(unpacked STREQUAL hash)
            message(STATUS "Have ${name}")
            continue()
        endif()
    endif()
    set(package "${downloads}/${name}")
    if(NOT EXISTS "${package}")
        legilo_download("${url}" "${package}" "${hash}")
    endif()
    message(STATUS "Unpacking ${name}")
    set(work "${word_lists}/.unpack")
    file(REMOVE_RECURSE "${work}")
    file(ARCHIVE_EXTRACT INPUT "${package}" DESTINATION "${work}")
    file(GLOB data "${work}/data.tar.*")
    file(ARCHIVE_EXTRACT INPUT "${data}" DESTINATION "${work}/data")
    file(GLOB lists "${work}/data/usr/share/dict/*-english")
    if(NOT lists)
        message(FATAL_ERROR "${name} holds no word list")
    endif()
    file(COPY ${lists} DESTINATION "${word_lists}")
    file(REMOVE_RECURSE "${work}")
    file(WRITE "${stamp}" "${hash}")
endforeach()

if(NOT LEGILO_TEST_VOICES)
    return()
endif()

# Each voice unpacks to a directory named after its archive. A stamp file
# records the archive's hash, so a changed pin unpacks afresh. The archives
# are deleted once unpacked, as is their espeak-ng data: Legilo uses the
# system's.
set(voices "${third_party}/test-voices")
file(MAKE_DIRECTORY "${voices}")
foreach(entry IN LISTS LEGILO_TEST_VOICES)
    legilo_dependency_fields("${entry}" name url hash)
    string(REGEX REPLACE "\\.tar\\.bz2$" "" directory "${name}")
    set(stamp "${voices}/${directory}/.legilo-sha256")
    if(EXISTS "${stamp}")
        file(READ "${stamp}" unpacked)
        if(unpacked STREQUAL hash)
            message(STATUS "Have ${directory}")
            continue()
        endif()
    endif()
    file(REMOVE_RECURSE "${voices}/${directory}")
    legilo_download("${url}" "${voices}/${name}" "${hash}")
    message(STATUS "Unpacking ${name}")
    file(ARCHIVE_EXTRACT INPUT "${voices}/${name}" DESTINATION "${voices}")
    file(REMOVE "${voices}/${name}")
    file(REMOVE_RECURSE "${voices}/${directory}/espeak-ng-data")
    file(WRITE "${stamp}" "${hash}")
endforeach()
