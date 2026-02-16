#pragma once

#include <uxdi/IDetector.h>
#include <uxdi/IDetectorListener.h>
#include <uxdi/DetectorFactory.h>
#include <uxdi/DetectorManager.h>
#include <uxdi/Types.h>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <chrono>
#include <mutex>
#include <atomic>

// Forward declarations for DirectX
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

// Forward declarations for ImGui
struct ImVec4;

namespace uxdi::gui_demo {

// Forward declaration
class GUIDemoApp;

/**
 * @brief Frame data for display
 */
struct DisplayFrame {
    std::shared_ptr<uint8_t[]> data;
    uint32_t width;
    uint32_t height;
    uint32_t bitDepth;
    uint64_t frameNumber;
    double timestamp;

    DisplayFrame() : width(0), height(0), bitDepth(0), frameNumber(0), timestamp(0) {}
};

/**
 * @brief Demo listener for receiving frames
 */
class DemoListener : public IDetectorListener {
public:
    explicit DemoListener(GUIDemoApp* app = nullptr) : app_(app) {}

    void onImageReceived(const ImageData& image) override;
    void onStateChanged(DetectorState newState) override;
    void onError(const ErrorInfo& error) override;
    void onAcquisitionStarted() override;
    void onAcquisitionStopped() override;

    DisplayFrame getLatestFrame() const;
    uint64_t getReceivedFrameCount() const { return receivedFrameCount_; }
    float getCurrentFPS() const { return currentFPS_; }
    std::string getLastError() const;
    void clearLastError();

    void updateFPS();

private:
    DisplayFrame latestFrame_;
    mutable std::mutex frameMutex_;
    uint64_t receivedFrameCount_ = 0;
    float currentFPS_ = 0.0f;
    std::chrono::steady_clock::time_point lastFPSCalculation_ = std::chrono::steady_clock::now();
    uint64_t framesSinceLastCalculation_ = 0;
    std::string lastError_;
    GUIDemoApp* app_;  // Non-owning pointer to parent app
};

/**
 * @brief GUI Demo Application
 */
class GUIDemoApp {
public:
    GUIDemoApp();
    ~GUIDemoApp();

    bool initialize(HWND hwnd, int width, int height);
    void shutdown();

    void run();
    void newFrame();
    void render();

    void handleResize(int width, int height);

    // Keyboard shortcuts
    void toggleDemoWindow() { showDemoWindow_ = !showDemoWindow_; }
    void toggleHelpWindow() { showHelpWindow_ = !showHelpWindow_; }
    void toggleAcquisition();
    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    void renderMainMenu();
    void renderAdapterPanel();
    void renderControlPanel();
    void renderStatusPanel();
    void renderImageDisplay();
    void renderFPSGraph();
    void renderHelpWindow();

    void loadAdapter(const std::string& dllName);
    void createDetector();
    void destroyDetector();
    void startAcquisition();
    void stopAcquisition();

    bool createDirectXResources();
    void cleanupDirectXResources();
    bool createDisplayTexture(uint32_t width, uint32_t height);
    void updateTextureData(const DisplayFrame& frame);

    HWND hwnd_ = nullptr;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;

    ID3D11Device* d3dDevice_ = nullptr;
    ID3D11DeviceContext* d3dContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* renderTargetView_ = nullptr;
    ID3D11Texture2D* displayTexture_ = nullptr;
    ID3D11ShaderResourceView* displayTextureSRV_ = nullptr;
    uint32_t textureWidth_ = 0;
    uint32_t textureHeight_ = 0;

    std::unique_ptr<DetectorManager> detectorManager_;
    std::shared_ptr<DemoListener> listener_;
    size_t currentAdapterId_ = 0;
    size_t detectorId_ = 0;
    IDetector* detector_ = nullptr;  // Raw pointer, owned by DetectorManager

    int selectedAdapterIndex_ = 0;
    bool showDemoWindow_ = false;
    bool showImageWindow_ = true;
    bool showHelpWindow_ = false;
    float clearColor_[4] = {0.1f, 0.1f, 0.1f, 1.0f};

    std::array<float, 120> fpsHistory_{};
    size_t fpsHistoryIndex_ = 0;

    DetectorInfo detectorInfo_;
    std::string lastError_;
};

} // namespace uxdi::gui_demo
