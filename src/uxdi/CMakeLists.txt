# uxdi_core - Core framework static library
# Contains detector interface definitions and factory/manager classes

set(UXDI_CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uxdi/Types.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/IDetector.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/IDetectorListener.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/IDetectorSynchronous.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/uxdi_export.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/DetectorFactory.h
    ${CMAKE_SOURCE_DIR}/include/uxdi/DetectorManager.h
)

set(UXDI_CORE_SOURCES
    DetectorFactory.cpp
    DetectorManager.cpp
)

add_library(uxdi_core STATIC
    ${UXDI_CORE_HEADERS}
    ${UXDI_CORE_SOURCES}
)

target_include_directories(uxdi_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(uxdi_core PUBLIC cxx_std_20)

# uxdi_core is a static library, define UXDI_STATIC_DEFINE to disable DLL export/import
target_compile_definitions(uxdi_core PUBLIC UXDI_STATIC_DEFINE)

# Windows-specific settings
if(WIN32)
    target_compile_definitions(uxdi_core PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()
