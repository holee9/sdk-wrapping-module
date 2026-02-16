# UXDI Test Suite
# Unit tests for core framework and adapters using GoogleTest

# Enable testing
enable_testing()

set(TEST_COMMON_SOURCES
    # Common test utilities will go here
)

# Core framework tests
set(CORE_TEST_SOURCES
    test_core/test_detector_types.cpp
    test_core/test_detector_factory.cpp
    test_core/test_detector_manager.cpp
)

add_executable(uxdi_core_tests
    ${CORE_TEST_SOURCES}
)

target_link_libraries(uxdi_core_tests
    PRIVATE
        uxdi_core
        gtest
        gtest_main
)

target_include_directories(uxdi_core_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(uxdi_core_tests PRIVATE cxx_std_20)

if(WIN32)
    target_compile_definitions(uxdi_core_tests PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Register core tests
include(GoogleTest)
gtest_discover_tests(uxdi_core_tests)

# Adapter tests (placeholder - will be implemented in Task 9, 15)
message(STATUS "Adapter tests will be added in Task 9 and Task 15")

# ============================================================================
# Integration Tests
# ============================================================================
add_subdirectory(integration)
