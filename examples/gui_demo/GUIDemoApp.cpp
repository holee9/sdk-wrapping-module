#include "GUIDemoApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <iostream>
#include <cstdio>

using namespace uxdi;
using namespace uxdi::gui_demo;

//=============================================================================
// Helper Functions
//=============================================================================

static const char* stateToString(DetectorState state) {
    switch (static_cast<int>(state)) {
        case 0: return "UNKNOWN";      // DetectorState::UNKNOWN
        case 1: return "IDLE";         // DetectorState::IDLE
        case 2: return "INITIALIZING"; // DetectorState::INITIALIZING
        case 3: return "READY";        // DetectorState::READY
        case 4: return "ACQUIRING";    // DetectorState::ACQUIRING
        case 5: return "STOPPING";     // DetectorState::STOPPING
        case 6: return "ERROR";        // DetectorState::ERROR
        default: return "INVALID";
    }
}

//=============================================================================
// DemoListener Implementation
//=============================================================================

void DemoListener::onImageReceived(const ImageData& image) {
    std::lock_guard<std::mutex> lock(frameMutex_);

    latestFrame_.width = image.width;
    latestFrame_.height = image.height;
    latestFrame_.bitDepth = image.bitDepth;
    latestFrame_.frameNumber = image.frameNumber;
    latestFrame_.timestamp = image.timestamp;

    size_t dataSize = image.width * image.height * (image.bitDepth <= 8 ? 1 : 2);
    latestFrame_.data = std::shared_ptr<uint8_t[]>(new uint8_t[dataSize],
        std::default_delete<uint8_t[]>());
    memcpy(latestFrame_.data.get(), image.data.get(), dataSize);

    receivedFrameCount_++;
    framesSinceLastCalculation_++;
}

void DemoListener::onStateChanged(DetectorState newState) {
    // State changes are handled via polling in the main loop
}

void DemoListener::onError(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    lastError_ = error.message;
    std::cout << "[Error] " << error.message << std::endl;
}

std::string DemoListener::getLastError() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return lastError_;
}

void DemoListener::clearLastError() {
    std::lock_guard<std::mutex> lock(frameMutex_);
    lastError_.clear();
}

void DemoListener::onAcquisitionStarted() {
    // Acquisition started - state changes are handled via polling in the main loop
}

void DemoListener::onAcquisitionStopped() {
    // Acquisition stopped - state changes are handled via polling in the main loop
}

DisplayFrame DemoListener::getLatestFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return latestFrame_;
}

void DemoListener::updateFPS() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSCalculation_).count();

    if (elapsed >= 500) {
        currentFPS_ = framesSinceLastCalculation_ * 1000.0f / elapsed;
        framesSinceLastCalculation_ = 0;
        lastFPSCalculation_ = now;
    }
}

//=============================================================================
// GUIDemoApp Implementation
//=============================================================================

GUIDemoApp::GUIDemoApp()
    : detectorManager_(std::make_unique<DetectorManager>())
    , listener_(std::make_shared<DemoListener>(this))
{
}

GUIDemoApp::~GUIDemoApp() {
    shutdown();
}

bool GUIDemoApp::initialize(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    windowWidth_ = width;
    windowHeight_ = height;

    std::cout << "[DEBUG] Initializing GUIDemoApp..." << std::endl;

    if (!createDirectXResources()) {
        std::cerr << "[ERROR] Failed to create DirectX resources" << std::endl;
        return false;
    }
    std::cout << "[DEBUG] DirectX resources created" << std::endl;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Set display size explicitly
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::StyleColorsDark();

    // Initialize backends FIRST (they handle font texture creation)
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(d3dDevice_, d3dContext_);

    // Load fonts (backend will create texture automatically)
    io.Fonts->AddFontDefault();

    // Create device objects to upload font texture to GPU
    bool created = ImGui_ImplDX11_CreateDeviceObjects();
    std::cout << "[DEBUG] Device objects created: " << (created ? "YES" : "NO") << std::endl;

    std::cout << "[DEBUG] ImGui initialized successfully" << std::endl;

    return true;
}

void GUIDemoApp::shutdown() {
    cleanupDirectXResources();

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    destroyDetector();

    if (detectorManager_) {
        detectorManager_->DestroyAllDetectors();
    }

    DetectorFactory::UnloadAllAdapters();
}

void GUIDemoApp::run() {
    static int frameCount = 0;

    // Update ImGui IO
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)windowWidth_, (float)windowHeight_);
    io.DeltaTime = 1.0f / 60.0f;

    // Set render target and viewport
    if (renderTargetView_) {
        d3dContext_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    }
    D3D11_VIEWPORT vp = {};
    vp.Width = (FLOAT)windowWidth_;
    vp.Height = (FLOAT)windowHeight_;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    d3dContext_->RSSetViewports(1, &vp);

    // Ensure proper blend state for ImGui (transparent rendering)
    static ID3D11BlendState* blendState = nullptr;
    if (!blendState) {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        d3dDevice_->CreateBlendState(&desc, &blendState);
    }
    d3dContext_->OMSetBlendState(blendState, nullptr, 0xffffffff);

    // Start new ImGui frame - ORDER MATTERS!
    // Win32 first (for input), then DX11, then NewFrame
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    // Clear render target
    d3dContext_->ClearRenderTargetView(renderTargetView_, clearColor_);

    // Update FPS
    if (listener_) {
        listener_->updateFPS();

        // Update FPS history for graph
        if (frameCount % 30 == 0) {  // Update every 30 frames
            fpsHistory_[fpsHistoryIndex_] = listener_->getCurrentFPS();
            fpsHistoryIndex_ = (fpsHistoryIndex_ + 1) % fpsHistory_.size();
        }
    }

    // Build UI - Render all panels
    // IMPORTANT: MainMenuBar must be rendered FIRST!

    // Show ImGui demo window by default (it's known to work)
    static bool showDemo = true;
    if (showDemo) {
        ImGui::ShowDemoWindow(&showDemo);
    }

    // MINIMAL TEST: Draw ImGui directly to screen to verify rendering
    {
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Once);
        ImGui::Begin("MINIMAL TEST - Should be VISIBLE!", nullptr, 0);
        ImGui::Text("If you see this window, ImGui rendering works!");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "RED TEXT");
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "GREEN TEXT");
        ImGui::TextColored(ImVec4(0, 0, 1, 1), "BLUE TEXT");
        ImGui::Separator();
        ImGui::Text("Frame: %d", frameCount);
        ImGui::Text("Window: %dx%d", windowWidth_, windowHeight_);
        if (ImGui::Button("Test Button - Click Me!")) {
            std::cout << "Button clicked at frame " << frameCount << std::endl;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Show ImGui Demo", &showDemo);
        ImGui::End();
    }

    // Render main menu bar
    float menuBarHeight = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                PostMessage(hwnd_, WM_CLOSE, 0, 0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Image Window", nullptr, &showImageWindow_);
            ImGui::MenuItem("Demo Window", nullptr, &showDemoWindow_);
            ImGui::MenuItem("Help Window (F1)", nullptr, &showHelpWindow_);
            ImGui::EndMenu();
        }
        menuBarHeight = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
    }

    // Render panels below menu bar
    ImGui::SetNextWindowPos(ImVec2(450, 20), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280, 320), ImGuiCond_Once);
    renderAdapterPanel();

    renderControlPanel();
    renderStatusPanel();
    renderImageDisplay();

    // Show help window if enabled
    if (showHelpWindow_) {
        renderHelpWindow();
    }

    // Render
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();

    // Debug output for first few frames
    if (frameCount < 10) {
        std::cout << "[DEBUG] Frame " << frameCount << ": drawData=" << drawData
                  << ", CmdListsCount=" << (drawData ? drawData->CmdListsCount : -1)
                  << ", TotalVtxCount=" << (drawData ? drawData->TotalVtxCount : -1)
                  << std::endl;
    }

    if (drawData && drawData->CmdListsCount > 0) {
        ImGui_ImplDX11_RenderDrawData(drawData);
    }

    swapChain_->Present(1, 0);

    frameCount++;
}

void GUIDemoApp::renderAdapterPanel() {
    // Note: Position is set by caller for testing
    if (ImGui::Begin("Adapters", nullptr, ImGuiWindowFlags_None)) {
        ImGui::Text("Select an adapter to load:");

        static const char* adapterNames[] = {
            "Dummy Adapter",
            "Emulator Adapter",
            "Varex Adapter",
            "Vieworks Adapter",
            "ABYZ Adapter"
        };

        ImGui::Combo("##Adapter", &selectedAdapterIndex_, adapterNames, 5);

        ImGui::Spacing();

        if (ImGui::Button("Load Adapter", ImVec2(150, 0))) {
            const char* dllNames[] = {
                "uxdi_dummy.dll",
                "uxdi_emul.dll",
                "uxdi_varex.dll",
                "uxdi_vieworks.dll",
                "uxdi_abyz.dll"
            };
            loadAdapter(dllNames[selectedAdapterIndex_]);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Show loaded adapters
        ImGui::Text("Loaded Adapters:");
        auto loadedAdapters = DetectorFactory::GetLoadedAdapters();

        if (loadedAdapters.empty()) {
            ImGui::TextDisabled("  None");
        } else {
            ImGui::Indent();
            for (const auto& info : loadedAdapters) {
                ImGui::Text("  %s", info.name.c_str());
            }
            ImGui::Unindent();
        }
    }
    ImGui::End();
}

void GUIDemoApp::renderControlPanel() {
    ImGui::SetNextWindowPos(ImVec2(300, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Detector Control", nullptr, ImGuiWindowFlags_None)) {
        if (detectorId_ == 0) {
            ImGui::TextDisabled("No detector created");
        } else if (detector_) {
            DetectorState state = detector_->getState();
            const char* stateStr = stateToString(state);
            ImVec4 stateColor = (static_cast<int>(state) == 6) ?
                ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            ImGui::TextColored(stateColor, "State: %s", stateStr);

            ImGui::Spacing();

            bool isAcquiring = detector_->isAcquiring();

            if (static_cast<int>(state) == 1) { // IDLE
                if (ImGui::Button("Initialize", ImVec2(120, 0))) {
                    if (detector_->initialize()) {
                        detectorInfo_ = detector_->getDetectorInfo();
                        detectorManager_->AddListener(detectorId_, listener_.get());
                    }
                }
            } else if (static_cast<int>(state) == 3 || static_cast<int>(state) == 6) { // READY or ERROR
                if (!isAcquiring) {
                    if (ImGui::Button("Start Acquisition", ImVec2(160, 0))) {
                        startAcquisition();
                    }
                } else {
                    if (ImGui::Button("Stop Acquisition", ImVec2(160, 0))) {
                        stopAcquisition();
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Shutdown", ImVec2(120, 0))) {
                    detector_->shutdown();
                }
            } else if (static_cast<int>(state) == 4) { // ACQUIRING
                if (ImGui::Button("Stop Acquisition", ImVec2(160, 0))) {
                    stopAcquisition();
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (currentAdapterId_ != 0) {
            if (detectorId_ == 0) {
                if (ImGui::Button("Create Detector", ImVec2(150, 0))) {
                    createDetector();
                }
            } else {
                if (ImGui::Button("Destroy Detector", ImVec2(150, 0))) {
                    destroyDetector();
                }
            }
        }
    }
    ImGui::End();
}

void GUIDemoApp::renderStatusPanel() {
    ImGui::SetNextWindowPos(ImVec2(590, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 220), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Status", nullptr, ImGuiWindowFlags_None)) {
        // Show error message if present
        std::string listenerError = listener_->getLastError();
        if (!listenerError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[ERROR]");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", listenerError.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Clear Error")) {
                listener_->clearLastError();
            }
            ImGui::Separator();
            ImGui::Spacing();
        } else if (!lastError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[ERROR]");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", lastError_.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Clear Error")) {
                lastError_.clear();
            }
            ImGui::Separator();
            ImGui::Spacing();
        }

        if (detectorId_ != 0 && detector_) {
            ImGui::Text("Detector Info:");
            ImGui::Indent();
            ImGui::Text("Vendor: %s", detector_->getVendorName().c_str());
            ImGui::Text("Model: %s", detector_->getModelName().c_str());
            ImGui::Unindent();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Acquisition:");
            ImGui::Indent();
            ImGui::Text("Frames Received: %llu", listener_->getReceivedFrameCount());
            ImGui::Text("Current FPS: %.1f", listener_->getCurrentFPS());
            ImGui::Unindent();
        } else {
            ImGui::TextDisabled("No detector created");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Keyboard shortcuts help
        ImGui::Text("Keyboard Shortcuts:");
        ImGui::Indent();
        ImGui::TextDisabled("F1 - Toggle Demo Window");
        ImGui::TextDisabled("F5 - Start/Stop Acquisition");
        ImGui::TextDisabled("ESC - Exit");
        ImGui::Unindent();
    }
    ImGui::End();
}

void GUIDemoApp::renderImageDisplay() {
    if (!showImageWindow_) return;

    ImGui::SetNextWindowPos(ImVec2(300, 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(570, 450), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Image Display", &showImageWindow_)) {
        renderFPSGraph();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DisplayFrame frame = listener_->getLatestFrame();
        if (frame.data && frame.width > 0 && frame.height > 0) {
            updateTextureData(frame);

            ImVec2 availSize = ImGui::GetContentRegionAvail();
            float aspect = (float)frame.width / frame.height;
            ImVec2 imageSize;

            if (availSize.x / availSize.y > aspect) {
                imageSize.y = availSize.y;
                imageSize.x = availSize.y * aspect;
            } else {
                imageSize.x = availSize.x;
                imageSize.y = availSize.x / aspect;
            }

            ImGui::Image((void*)displayTextureSRV_, imageSize);

            ImGui::Text("Frame: %llu | %dx%d | %d-bit",
                frame.frameNumber, frame.width, frame.height, frame.bitDepth);
        } else {
            ImVec2 availSize = ImGui::GetContentRegionAvail();
            ImGui::Dummy(availSize);
            ImGui::TextDisabled("No image data");
        }
    }
    ImGui::End();
}

void GUIDemoApp::renderFPSGraph() {
    ImGui::Text("Frame Rate History:");
    float avgFPS = 0;
    int validSamples = 0;
    for (float fps : fpsHistory_) {
        if (fps > 0) {
            avgFPS += fps;
            validSamples++;
        }
    }
    if (validSamples > 0) avgFPS /= validSamples;

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Avg: %.1f FPS", avgFPS);

    char overlay[32];
    snprintf(overlay, sizeof(overlay), "%.1f FPS", listener_->getCurrentFPS());
    ImGui::PlotLines("##FPS", fpsHistory_.data(), fpsHistory_.size(),
        (int)fpsHistoryIndex_, overlay, 0.0f, 100.0f, ImVec2(0, 80));
}

void GUIDemoApp::loadAdapter(const std::string& dllName) {
    try {
        std::wstring widePath = DetectorFactory::ToWideString(dllName);
        currentAdapterId_ = DetectorFactory::LoadAdapter(widePath);
        lastError_.clear();
        std::cout << "[INFO] Loaded adapter: " << dllName << " (ID: " << currentAdapterId_ << ")" << std::endl;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to load adapter ") + dllName + ": " + e.what();
        std::cerr << "[ERROR] " << lastError_ << std::endl;
    }
}

void GUIDemoApp::createDetector() {
    if (currentAdapterId_ == 0) {
        lastError_ = "No adapter loaded. Please load an adapter first.";
        std::cerr << "[ERROR] " << lastError_ << std::endl;
        return;
    }

    try {
        detectorId_ = detectorManager_->CreateDetector(currentAdapterId_, "");
        detector_ = detectorManager_->GetDetector(detectorId_);
        if (detector_) {
            detectorInfo_ = detector_->getDetectorInfo();
            detectorManager_->AddListener(detectorId_, listener_.get());
            lastError_.clear();
            std::cout << "[INFO] Created detector (ID: " << detectorId_ << ")" << std::endl;
        } else {
            lastError_ = "Failed to get detector instance after creation.";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
        }
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to create detector: ") + e.what();
        std::cerr << "[ERROR] " << lastError_ << std::endl;
    }
}

void GUIDemoApp::destroyDetector() {
    if (detectorId_ != 0) {
        if (detector_ && detector_->isAcquiring()) {
            detector_->stopAcquisition();
        }
        detectorManager_->DestroyDetector(detectorId_);
        detector_ = nullptr;
        detectorId_ = 0;
        std::cout << "[INFO] Detector destroyed" << std::endl;
    }
}

void GUIDemoApp::startAcquisition() {
    if (detectorId_ == 0 || !detector_) {
        lastError_ = "No detector available. Please create a detector first.";
        std::cerr << "[ERROR] " << lastError_ << std::endl;
        return;
    }

    if (detector_->startAcquisition()) {
        lastError_.clear();
        std::cout << "[INFO] Acquisition started" << std::endl;
    } else {
        lastError_ = "Failed to start acquisition.";
        std::cerr << "[ERROR] " << lastError_ << std::endl;
    }
}

void GUIDemoApp::stopAcquisition() {
    if (detectorId_ == 0 || !detector_) {
        lastError_ = "No detector available.";
        std::cerr << "[ERROR] " << lastError_ << std::endl;
        return;
    }

    if (detector_->stopAcquisition()) {
        lastError_.clear();
        std::cout << "[INFO] Acquisition stopped" << std::endl;
    } else {
        lastError_ = "Failed to stop acquisition.";
        std::cerr << "[ERROR] " << lastError_ << std::endl;
    }
}

void GUIDemoApp::toggleAcquisition() {
    if (detectorId_ == 0 || !detector_) return;

    DetectorState state = detector_->getState();
    if (static_cast<int>(state) == 4) {  // ACQUIRING
        stopAcquisition();
    } else {
        startAcquisition();
    }
}

bool GUIDemoApp::createDirectXResources() {
    D3D_FEATURE_LEVEL featureLevel;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = windowWidth_;
    sd.BufferDesc.Height = windowHeight_;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, nullptr, 0, D3D11_SDK_VERSION,
        &sd, &swapChain_, &d3dDevice_, &featureLevel, &d3dContext_);

    if (FAILED(hr)) {
        std::cerr << "[ERROR] D3D11CreateDeviceAndSwapChain failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    std::cout << "[DEBUG] Feature Level: " << featureLevel << std::endl;

    ID3D11Texture2D* pBackBuffer;
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr)) {
        std::cerr << "[ERROR] GetBuffer failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    hr = d3dDevice_->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView_);
    pBackBuffer->Release();

    if (FAILED(hr)) {
        std::cerr << "[ERROR] CreateRenderTargetView failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    std::cout << "[DEBUG] RenderTargetView created successfully" << std::endl;
    return true;
}

void GUIDemoApp::cleanupDirectXResources() {
    if (displayTextureSRV_) { displayTextureSRV_->Release(); displayTextureSRV_ = nullptr; }
    if (displayTexture_) { displayTexture_->Release(); displayTexture_ = nullptr; }
    if (renderTargetView_) { renderTargetView_->Release(); renderTargetView_ = nullptr; }
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (d3dContext_) { d3dContext_->Release(); d3dContext_ = nullptr; }
    if (d3dDevice_) { d3dDevice_->Release(); d3dDevice_ = nullptr; }
}

void GUIDemoApp::handleResize(int width, int height) {
    if (!d3dDevice_ || !swapChain_) return;

    windowWidth_ = width;
    windowHeight_ = height;

    std::cout << "[DEBUG] Handling resize: " << width << "x" << height << std::endl;

    // Release render target view
    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }

    // Resize swap chain buffers
    HRESULT hr = swapChain_->ResizeBuffers(
        0,  // BufferCount - 0 to preserve current setting
        width, height,
        DXGI_FORMAT_UNKNOWN,  // Format - DXGI_FORMAT_UNKNOWN to preserve current
        0  // Flags
    );

    if (SUCCEEDED(hr)) {
        // Recreate render target view
        ID3D11Texture2D* pBackBuffer;
        hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (SUCCEEDED(hr)) {
            hr = d3dDevice_->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView_);
            pBackBuffer->Release();
        }
    }

    if (FAILED(hr)) {
        std::cerr << "[ERROR] Resize failed, recreating resources" << std::endl;
        // Fallback: recreate everything
        cleanupDirectXResources();
        createDirectXResources();
        // Note: this will invalidate ImGui device objects, so we need to recreate them
        ImGui_ImplDX11_CreateDeviceObjects();
    }
}

bool GUIDemoApp::createDisplayTexture(uint32_t width, uint32_t height) {
    if (displayTexture_ && textureWidth_ == width && textureHeight_ == height) {
        return true;
    }

    if (displayTextureSRV_) { displayTextureSRV_->Release(); displayTextureSRV_ = nullptr; }
    if (displayTexture_) { displayTexture_->Release(); displayTexture_ = nullptr; }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(d3dDevice_->CreateTexture2D(&desc, nullptr, &displayTexture_))) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(d3dDevice_->CreateShaderResourceView(displayTexture_, &srvDesc, &displayTextureSRV_))) {
        return false;
    }

    textureWidth_ = width;
    textureHeight_ = height;
    return true;
}

void GUIDemoApp::updateTextureData(const DisplayFrame& frame) {
    if (!displayTexture_ || !d3dContext_) return;

    if (!createDisplayTexture(frame.width, frame.height)) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(d3dContext_->Map(displayTexture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        if (frame.bitDepth > 8) {
            const uint16_t* src = (const uint16_t*)frame.data.get();
            uint8_t* dst = (uint8_t*)mapped.pData;
            for (uint32_t y = 0; y < frame.height; ++y) {
                for (uint32_t x = 0; x < frame.width; ++x) {
                    dst[y * mapped.RowPitch + x] = (uint8_t)(src[y * frame.width + x] >> 8);
                }
            }
        } else {
            const uint8_t* src = frame.data.get();
            uint8_t* dst = (uint8_t*)mapped.pData;
            for (uint32_t y = 0; y < frame.height; ++y) {
                memcpy(dst + y * mapped.RowPitch, src + y * frame.width, frame.width);
            }
        }
        d3dContext_->Unmap(displayTexture_, 0);
    }
}

void GUIDemoApp::renderHelpWindow() {
    ImGui::SetNextWindowPos(ImVec2(windowWidth_ / 2 - 250, windowHeight_ / 2 - 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("UXDI GUI Demo - Help", &showHelpWindow_)) {
        ImGui::Text("UXDI (Universal X-ray Detector Interface) Demo Application");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Getting Started:");
        ImGui::BulletText("1. Select an adapter from the Adapters panel");
        ImGui::BulletText("2. Click 'Load Adapter' to load the DLL");
        ImGui::BulletText("3. Click 'Create Detector' to create a detector instance");
        ImGui::BulletText("4. Click 'Initialize' to initialize the detector");
        ImGui::BulletText("5. Click 'Start Acquisition' to begin image acquisition");
        ImGui::Spacing();

        ImGui::Text("Keyboard Shortcuts:");
        ImGui::BulletText("F1 - Show/Hide this help window");
        ImGui::BulletText("F5 - Start/Stop acquisition (when detector is ready)");
        ImGui::BulletText("ESC - Exit the application");
        ImGui::Spacing();

        ImGui::Text("Panels:");
        ImGui::BulletText("Adapters - Load and manage detector adapters");
        ImGui::BulletText("Detector Control - Initialize and control acquisition");
        ImGui::BulletText("Status - View detector information and statistics");
        ImGui::BulletText("Image Display - View acquired X-ray images");
        ImGui::Spacing();

        ImGui::Text("Available Adapters:");
        ImGui::BulletText("Dummy Adapter - Mock detector for testing");
        ImGui::BulletText("Emulator Adapter - Simulated X-ray source");
        ImGui::BulletText("Varex Adapter - Varex X-ray detectors");
        ImGui::BulletText("Vieworks Adapter - Vieworks X-ray detectors");
        ImGui::BulletText("ABYZ Adapter - ABYZ (Rayence/Samsung/DRTech) detectors");
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Text("For more information, see the UXDI documentation.");
    }
    ImGui::End();
}
