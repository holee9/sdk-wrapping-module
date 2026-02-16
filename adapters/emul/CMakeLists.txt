# uxdi_emul - Emulation adapter with ScenarioEngine
# Implements IDetector interface with configurable test scenarios

set(EMUL_ADAPTER_SOURCES
    src/EmulAdapter.cpp
    src/EmulDetector.cpp
    src/ScenarioEngine.cpp
)

add_library(uxdi_emul SHARED
    ${EMUL_ADAPTER_SOURCES}
)

target_include_directories(uxdi_emul
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(uxdi_emul
    PRIVATE
        uxdi_core
)

target_compile_features(uxdi_emul PRIVATE cxx_std_20)

# Windows DLL export definitions
if(WIN32)
    target_compile_definitions(uxdi_emul PRIVATE
        UXDI_EMUL_EXPORTS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Output naming
set_target_properties(uxdi_emul PROPERTIES
    OUTPUT_NAME uxdi_emul
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)
