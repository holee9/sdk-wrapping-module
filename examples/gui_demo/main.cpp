#include "GUIDemoApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <windows.h>
#include <d3d11.h>
#include <iostream>

// Forward declarations
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global application instance
uxdi::gui_demo::GUIDemoApp* g_app = nullptr;
int g_windowWidth = 1280;
int g_windowHeight = 720;

//=============================================================================
// Main Entry Point
//=============================================================================
int main(int argc, char* argv[]) {
    std::cout << "=== UXDI GUI Demo Application ===" << std::endl;
    std::cout << "Initializing..." << std::endl;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"UXDIDemoApp";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Create main window
    HWND hwnd = CreateWindowW(
        L"UXDIDemoApp",
        L"UXDI Detector Demo",
        WS_OVERLAPPEDWINDOW,
        100, 100,  // Explicit position
        g_windowWidth, g_windowHeight,
        nullptr, nullptr,
        wc.hInstance, nullptr
    );

    if (!hwnd) {
        std::cerr << "[ERROR] Failed to create window" << std::endl;
        return -1;
    }

    std::cout << "[DEBUG] Window created: HWND=" << hwnd << std::endl;

    // Show window FIRST (triggers WM_SIZE)
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // Process any pending messages (especially WM_SIZE)
    MSG initMsg;
    while (PeekMessage(&initMsg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&initMsg);
        DispatchMessage(&initMsg);
    }

    // Now initialize application (after window size is finalized)
    g_app = new uxdi::gui_demo::GUIDemoApp();
    if (!g_app->initialize(hwnd, g_windowWidth, g_windowHeight)) {
        std::cerr << "[ERROR] Failed to initialize application" << std::endl;
        delete g_app;
        DestroyWindow(hwnd);
        return -1;
    }

    std::cout << "[INFO] Application initialized successfully" << std::endl;
    std::cout << "[INFO] Use the GUI panels to load adapters and control detectors" << std::endl;

    // Main message loop with real-time rendering
    MSG msg;
    bool done = false;
    while (!done) {
        // Process all pending messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                done = true;
                break;
            }
        }

        if (done) break;

        // Render the frame
        if (g_app) {
            g_app->run();
        }
    }

    // Cleanup
    std::cout << "[INFO] Shutting down..." << std::endl;
    delete g_app;
    g_app = nullptr;

    // Destroy window
    DestroyWindow(hwnd);
    UnregisterClassW(L"UXDIDemoApp", wc.hInstance);

    return 0;
}

//=============================================================================
// Window Procedure
//=============================================================================
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && g_app) {
            g_windowWidth = LOWORD(lParam);
            g_windowHeight = HIWORD(lParam);
            g_app->handleResize(g_windowWidth, g_windowHeight);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) {  // Disable ALT application menu
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        // Handle keyboard shortcuts
        if (wParam == VK_F1) {
            // Toggle help window
            if (g_app) {
                g_app->toggleHelpWindow();
            }
        } else if (wParam == VK_F5) {
            // Toggle acquisition
            if (g_app) {
                g_app->toggleAcquisition();
            }
        } else if (wParam == VK_ESCAPE) {
            // Confirm exit
            if (MessageBoxW(hwnd, L"Exit UXDI Demo?", L"Confirm",
                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
