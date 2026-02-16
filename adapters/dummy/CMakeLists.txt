# uxdi_dummy - Dummy adapter for testing without hardware
# Implements IDetector interface with simulated behavior

set(DUMMY_ADAPTER_SOURCES
    src/DummyAdapter.cpp
    src/DummyDetector.cpp
)

add_library(uxdi_dummy SHARED
    ${DUMMY_ADAPTER_SOURCES}
)

target_include_directories(uxdi_dummy
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(uxdi_dummy
    PRIVATE
        uxdi_core
)

target_compile_features(uxdi_dummy PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_dummy PRIVATE
        UXDI_DUMMY_EXPORTS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_dummy PROPERTIES
    OUTPUT_NAME uxdi_dummy
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
