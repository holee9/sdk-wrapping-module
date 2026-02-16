# UXDI GUI Demo Application using Dear ImGui + DirectX 11

cmake_minimum_required(VERSION 3.20)

# GUI Demo executable
add_executable(gui_demo)

# Set C++20
target_compile_features(gui_demo PUBLIC cxx_std_20)

# Source files
target_sources(gui_demo
    PRIVATE
        main.cpp
        GUIDemoApp.cpp
        GUIDemoApp.h
)

# Include directories
target_include_directories(gui_demo
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link UXDI core
target_link_libraries(gui_demo
    PRIVATE
        uxdi_core
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(gui_demo
        PRIVATE
            d3d11
            d3dcompiler
            dxgi
            dxguid
    )
endif()

# Fetch Dear ImGui
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        master  # Use master branch
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)

# Make ImGui available
FetchContent_MakeAvailable(imgui)

# Get ImGui source directory
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# Check if ImGui was downloaded successfully
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(WARNING "ImGui sources not found, trying alternative approach...")
    # Fallback: download manually
    set(IMGUI_DIR "${CMAKE_BINARY_DIR}/_deps/imgui-src")
    if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
        message(STATUS "Downloading ImGui to ${IMGUI_DIR}")
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui.cpp
            ${IMGUI_DIR}/imgui.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui.h
            ${IMGUI_DIR}/imgui.h
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui_widgets.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imgui_internal.h
            ${IMGUI_DIR}/imgui_internal.h
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imstb_textedit.h
            ${IMGUI_DIR}/imstb_textedit.h
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imstb_rectpack.h
            ${IMGUI_DIR}/imstb_rectpack.h
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/imstb_truetype.h
            ${IMGUI_DIR}/imstb_truetype.h
        )
        # Download backends
        file(MAKE_DIRECTORY ${IMGUI_DIR}/backends)
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_win32.cpp
            ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_win32.h
            ${IMGUI_DIR}/backends/imgui_impl_win32.h
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_dx11.cpp
            ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
        )
        file(DOWNLOAD
            https://raw.githubusercontent.com/ocornut/imgui/docking/backends/imgui_impl_dx11.h
            ${IMGUI_DIR}/backends/imgui_impl_dx11.h
        )
    endif()
endif()

# Add ImGui library if sources exist
if(EXISTS "${IMGUI_DIR}/imgui.cpp")
    add_library(imgui_lib STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    )

    target_include_directories(imgui_lib
        PUBLIC
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
    )

    target_compile_features(imgui_lib PUBLIC cxx_std_20)

    # Link ImGui to GUI demo
    target_link_libraries(gui_demo
        PRIVATE
            imgui_lib
    )
else()
    message(FATAL_ERROR "Failed to download ImGui sources")
endif()

# Windows subsystem:GUI (remove console window)
# Note: Using console mode for easier debugging
if(WIN32 AND CMAKE_MSVC)
    # Keep console for debugging output
    # set_target_properties(gui_demo PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Copy DLLs to output directory
add_custom_command(TARGET gui_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uxdi_dummy>
        $<TARGET_FILE_DIR:gui_demo>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uxdi_emul>
        $<TARGET_FILE_DIR:gui_demo>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uxdi_varex>
        $<TARGET_FILE_DIR:gui_demo>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uxdi_vieworks>
        $<TARGET_FILE_DIR:gui_demo>/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:uxdi_abyz>
        $<TARGET_FILE_DIR:gui_demo>/
    COMMENT "Copying adapter DLLs to gui_demo output directory"
)
