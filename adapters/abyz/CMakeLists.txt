# uxdi_abyz - ABYZ (Rayence/Samsung/DRTech) X-ray detector adapter
# Implements IDetector interface for ABYZ vendor SDKs

set(ABYZ_ADAPTER_SOURCES
    src/ABYZAdapter.cpp
    src/ABYZDetector.cpp
)

# Mock SDK sources (for development/testing)
set(ABYZ_MOCK_SDK_SOURCES
    ${CMAKE_SOURCE_DIR}/mock_sdk/abyz/src/ABYZMockSDK.cpp
)

add_library(uxdi_abyz SHARED
    ${ABYZ_ADAPTER_SOURCES}
    ${ABYZ_MOCK_SDK_SOURCES}
)

target_include_directories(uxdi_abyz
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/mock_sdk/abyz/include
)

target_link_libraries(uxdi_abyz
    PRIVATE
        uxdi_core
)

target_compile_features(uxdi_abyz PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_abyz PRIVATE
        UXDI_ABYZ_EXPORTS
        ABYZ_MOCK_SDK_IMPL
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_abyz PROPERTIES
    OUTPUT_NAME uxdi_abyz
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
