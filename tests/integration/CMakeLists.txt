# Integration test for adapter loading
cmake_minimum_required(VERSION 3.20)

# Integration test executable
add_executable(test_adapter_load
    test_adapter_load.cpp
)

target_include_directories(test_adapter_load
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_adapter_load
    PRIVATE
        uxdi_core
)

target_compile_features(test_adapter_load PRIVATE cxx_std_20)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(test_adapter_load PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
    # Set output directory to bin folder for DLL access
    set_target_properties(test_adapter_load PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
