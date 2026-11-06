# Benchmarks (design 9.6) are built with the tests but are not CTest
# tests: timing assertions are flaky, so scripts/bench runs them, records
# the results and reports regressions without failing.
add_executable(bench_pipeline bench_pipeline.cpp)
target_link_libraries(bench_pipeline PRIVATE legilo_core)
legilo_set_warnings(bench_pipeline)
target_compile_definitions(bench_pipeline PRIVATE
    LEGILO_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/corpus")
