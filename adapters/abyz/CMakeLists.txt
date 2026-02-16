# uxdi_abyz - ABYZ (Rayence/Samsung/DRTech) X-ray detector adapter
# Implements IDetector interface for ABYZ vendor SDKs

set(ABYZ_ADAPTER_SOURCES
    src/ABYZAdapter.cpp
)

add_library(uxdi_abyz SHARED
    ${ABYZ_ADAPTER_SOURCES}
)

target_include_directories(uxdi_abyz
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# TODO: Add ABYZ SDK include path when available
# target_include_directories(uxdi_abyz PRIVATE ${ABYZ_SDK_INCLUDE_DIRS})

target_link_libraries(uxdi_abyz
    PRIVATE
        uxdi_core
)

# TODO: Add ABYZ SDK libraries when available
# target_link_libraries(uxdi_abyz PRIVATE ${ABYZ_SDK_LIBRARIES})

target_compile_features(uxdi_abyz PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_abyz PRIVATE
        UXDI_ABYZ_EXPORTS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_abyz PROPERTIES
    OUTPUT_NAME uxdi_abyz
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
