find_package(Qt6 REQUIRED COMPONENTS Test)
# The installer tests build their archives with libarchive.
find_package(LibArchive 3.6 REQUIRED)

# legilo_add_unit_test(<name> SOURCES <files...> LIBS <targets...>)
function(legilo_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    # Shared test doubles, as "fakes/FakeTtsEngine.h".
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests)
    legilo_set_warnings(${name})
    target_compile_definitions(${name} PRIVATE
        LEGILO_FIXTURES_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${name} COMMAND ${name})
    # No test may need a display or touch the user's real settings. Unit
    # tests take well under a second, so a long run means a deadlock.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_LOGGING_RULES=*.debug=false"
        TIMEOUT 30)
endfunction()

legilo_add_unit_test(tst_settings SOURCES tst_settings.cpp LIBS legilo_app)
legilo_add_unit_test(tst_normaliser SOURCES tst_normaliser.cpp LIBS legilo_core)
legilo_add_unit_test(tst_pdfdetector SOURCES tst_pdfdetector.cpp LIBS legilo_core)
legilo_add_unit_test(tst_reflow SOURCES tst_reflow.cpp LIBS legilo_core)
legilo_add_unit_test(tst_wordlist SOURCES tst_wordlist.cpp LIBS legilo_core)
legilo_add_unit_test(tst_segmenter SOURCES tst_segmenter.cpp LIBS legilo_core)
legilo_add_unit_test(tst_phonemiser SOURCES tst_phonemiser.cpp LIBS legilo_core)
legilo_add_unit_test(tst_voiceregistry SOURCES tst_voiceregistry.cpp LIBS legilo_core)
legilo_add_unit_test(tst_sherpattsengine SOURCES tst_sherpattsengine.cpp LIBS legilo_core)
legilo_add_unit_test(tst_playbackcontroller SOURCES tst_playbackcontroller.cpp LIBS legilo_core)
legilo_add_unit_test(tst_firstchunk SOURCES tst_firstchunk.cpp LIBS legilo_core)
legilo_add_unit_test(tst_audioqueue SOURCES tst_audioqueue.cpp LIBS legilo_core)
legilo_add_unit_test(tst_modelinstaller SOURCES tst_modelinstaller.cpp
    LIBS legilo_core LibArchive::LibArchive)
