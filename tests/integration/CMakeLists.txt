find_package(Qt6 REQUIRED COMPONENTS Test)

# legilo_add_integration_test(<name> SOURCES <files...> LIBS <targets...>)
#
# Integration tests exercise real dependencies, such as sherpa-onnx with real
# voices from scripts/fetch-deps --test-voices, and skip what is not
# installed. They carry the "integration" label, so `ctest -LE integration`
# runs the unit tests alone.
function(legilo_add_integration_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    legilo_set_warnings(${name})
    target_compile_definitions(${name} PRIVATE
        LEGILO_TEST_VOICES_DIR="${PROJECT_SOURCE_DIR}/third_party/test-voices")
    add_test(NAME ${name} COMMAND ${name})
    # Loading and running models takes seconds, not milliseconds.
    set_tests_properties(${name} PROPERTIES
        LABELS integration
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_LOGGING_RULES=*.debug=false"
        TIMEOUT 300)
endfunction()

legilo_add_integration_test(tst_synthesis SOURCES tst_synthesis.cpp LIBS legilo_core)
