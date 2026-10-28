find_package(Qt6 REQUIRED COMPONENTS Test)

# legilo_add_unit_test(<name> SOURCES <files...> LIBS <targets...>)
function(legilo_add_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    legilo_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    # No test may need a display or touch the user's real settings. Unit
    # tests take well under a second, so a long run means a deadlock.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_LOGGING_RULES=*.debug=false"
        TIMEOUT 30)
endfunction()

legilo_add_unit_test(tst_settings SOURCES tst_settings.cpp LIBS legilo_app)
