# uxdi_vieworks - Vieworks X-ray detector adapter
# Implements IDetector interface for Vieworks SDK

set(VIEWORKS_ADAPTER_SOURCES
    src/VieworksAdapter.cpp
)

add_library(uxdi_vieworks SHARED
    ${VIEWORKS_ADAPTER_SOURCES}
)

target_include_directories(uxdi_vieworks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# TODO: Add Vieworks SDK include path when available
# target_include_directories(uxdi_vieworks PRIVATE ${VIEWORKS_SDK_INCLUDE_DIRS})

target_link_libraries(uxdi_vieworks
    PRIVATE
        uxdi_core
)

# TODO: Add Vieworks SDK libraries when available
# target_link_libraries(uxdi_vieworks PRIVATE ${VIEWORKS_SDK_LIBRARIES})

target_compile_features(uxdi_vieworks PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_vieworks PRIVATE
        UXDI_VIEWORKS_EXPORTS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_vieworks PROPERTIES
    OUTPUT_NAME uxdi_vieworks
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
