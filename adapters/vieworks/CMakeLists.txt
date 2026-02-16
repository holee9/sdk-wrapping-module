# uxdi_vieworks - Vieworks X-ray detector adapter
# Implements IDetector interface for Vieworks SDK

set(VIEWORKS_ADAPTER_SOURCES
    src/VieworksAdapter.cpp
    src/VieworksDetector.cpp
)

# Mock SDK sources (for development/testing)
set(VIEWORKS_MOCK_SDK_SOURCES
    ${CMAKE_SOURCE_DIR}/mock_sdk/vieworks/src/VieworksMockSDK.cpp
)

add_library(uxdi_vieworks SHARED
    ${VIEWORKS_ADAPTER_SOURCES}
    ${VIEWORKS_MOCK_SDK_SOURCES}
)

target_include_directories(uxdi_vieworks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/mock_sdk/vieworks/include
)

target_link_libraries(uxdi_vieworks
    PRIVATE
        uxdi_core
)

target_compile_features(uxdi_vieworks PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_vieworks PRIVATE
        UXDI_VIEWORKS_EXPORTS
        VIEWORKS_MOCK_SDK_IMPL
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_vieworks PROPERTIES
    OUTPUT_NAME uxdi_vieworks
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
