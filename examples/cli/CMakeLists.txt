# UXDI CLI Sample Application
# Command-line interface demonstrating UXDI usage

set(CLI_SOURCES
    uxdi_cli.cpp
)

add_executable(uxdi_cli
    ${CLI_SOURCES}
)

target_link_libraries(uxdi_cli
    PRIVATE
        uxdi_core
)

target_include_directories(uxdi_cli
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(uxdi_cli PRIVATE cxx_std_20)

if(WIN32)
    target_compile_definitions(uxdi_cli PRIVATE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Installation
install(TARGETS uxdi_cli
    RUNTIME DESTINATION bin
)
