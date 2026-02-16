# uxdi_varex - Varex X-ray detector adapter
# Implements IDetector interface for Varex SDK

set(VAREX_ADAPTER_SOURCES
    src/VarexAdapter.cpp
    src/VarexDetector.cpp
)

# Mock SDK sources (for development/testing)
set(VAREX_MOCK_SDK_SOURCES
    ${CMAKE_SOURCE_DIR}/mock_sdk/varex/src/VarexMockSDK.cpp
)

add_library(uxdi_varex SHARED
    ${VAREX_ADAPTER_SOURCES}
    ${VAREX_MOCK_SDK_SOURCES}
)

target_include_directories(uxdi_varex
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/mock_sdk/varex/include
)

target_link_libraries(uxdi_varex
    PRIVATE
        uxdi_core
)

target_compile_features(uxdi_varex PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_varex PRIVATE
        UXDI_VAREX_EXPORTS
        VAREX_MOCK_SDK_IMPL
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_varex PROPERTIES
    OUTPUT_NAME uxdi_varex
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
