# uxdi_varex - Varex X-ray detector adapter
# Implements IDetector interface for Varex SDK

set(VAREX_ADAPTER_SOURCES
    src/VarexAdapter.cpp
)

add_library(uxdi_varex SHARED
    ${VAREX_ADAPTER_SOURCES}
)

target_include_directories(uxdi_varex
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# TODO: Add Varex SDK include path when available
# target_include_directories(uxdi_varex PRIVATE ${VAREX_SDK_INCLUDE_DIRS})

target_link_libraries(uxdi_varex
    PRIVATE
        uxdi_core
)

# TODO: Add Varex SDK libraries when available
# target_link_libraries(uxdi_varex PRIVATE ${VAREX_SDK_LIBRARIES})

target_compile_features(uxdi_varex PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_varex PRIVATE
        UXDI_VAREX_EXPORTS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_varex PROPERTIES
    OUTPUT_NAME uxdi_varex
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
