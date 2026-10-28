# Abstract interfaces live in include/platform/; the implementations for
# the target platform are added to this library from linux/ or windows/.
add_library(legilo_platform STATIC
    Logging.cpp
    include/platform/Logging.h
)
target_include_directories(legilo_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(legilo_platform PUBLIC Qt6::Core)
legilo_set_warnings(legilo_platform)
