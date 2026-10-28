# Platform-independent logic: no Widgets and no direct platform API calls.
add_library(legilo_core STATIC
    Logging.cpp
    Logging.h
)
target_include_directories(legilo_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(legilo_core PUBLIC Qt6::Core)
legilo_set_warnings(legilo_core)
