#include "varex_sdk.h"
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

//=============================================================================
// Varex Mock SDK Implementation
//=============================================================================

namespace {

// Mock SDK state
static std::atomic<bool> g_sdkInitialized{false};
static std::mutex g_sdkMutex;

// Mock detector data
struct MockDetector {
    bool initialized = false;
    VarexState state = VAREX_STATE_IDLE;
    VarexAcqParams params{1024, 1024, 0, 0, 100.0f, 1.0f, 1};
    VarexDetectorInfo info{};
    uint64_t frameCounter = 0;
    bool acquiring = false;

    // Callbacks
    VarexImageCallback imageCallback = nullptr;
    VarexStateCallback stateCallback = nullptr;
    VarexErrorCallback errorCallback = nullptr;
    void* userContext = nullptr;

    // Frame generation thread
    std::atomic<bool> threadActive{false};
    std::thread frameThread;
};

static std::vector<MockDetector*> g_detectors;
static std::mutex g_detectorsMutex;

// Frame data buffer (owned by SDK, must be copied by adapter)
static std::vector<uint16_t> g_frameBuffer;

//=============================================================================
// Internal Helper Functions
//=============================================================================

VarexDetectorInfo createMockDetectorInfo() {
    VarexDetectorInfo info{};
    std::strncpy(info.vendor, "Varex", sizeof(info.vendor) - 1);
    std::strncpy(info.model, "Mock-4343CT", sizeof(info.model) - 1);
    std::strncpy(info.serialNumber, "VAREX-MOCK-001", sizeof(info.serialNumber) - 1);
    std::strncpy(info.firmwareVersion, "2.1.0", sizeof(info.firmwareVersion) - 1);
    info.maxWidth = 3072;
    info.maxHeight = 2048;
    info.bitDepth = 16;
    return info;
}

void frameGenerationThread(MockDetector* detector) {
    while (detector->threadActive.load() && detector->acquiring) {
        // Simulate ~30ms frame interval (~33 fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        if (!detector->threadActive.load() || !detector->acquiring) {
            break;
        }

        // Prepare frame data (SDK-owned buffer)
        const size_t pixelCount = detector->params.width * detector->params.height;
        if (g_frameBuffer.size() < pixelCount) {
            g_frameBuffer.resize(pixelCount);
        }

        // Generate mock frame data (gradient pattern for visualization)
        for (uint32_t y = 0; y < detector->params.height; ++y) {
            for (uint32_t x = 0; x < detector->params.width; ++x) {
                size_t idx = y * detector->params.width + x;
                // Create gradient pattern with some variation
                uint16_t value = static_cast<uint16_t>(
                    (x * 65535 / detector->params.width + y * 65535 / detector->params.height) / 2
                );
                // Add frame counter variation
                value = static_cast<uint16_t>((value + detector->frameCounter * 100) % 65536);
                g_frameBuffer[idx] = value;
            }
        }

        // Create image structure (SDK-owned memory)
        VarexImage image{};
        image.data = g_frameBuffer.data();
        image.width = detector->params.width;
        image.height = detector->params.height;
        image.bitDepth = 16;
        image.frameNumber = ++detector->frameCounter;
        image.timestamp = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        image.dataLength = static_cast<uint32_t>(pixelCount * sizeof(uint16_t));

        // Deliver image through callback (MUST copy immediately!)
        if (detector->imageCallback) {
            detector->imageCallback(&image, detector->userContext);
        }
    }
}

void startFrameThread(MockDetector* detector) {
    if (detector->threadActive.load()) {
        return; // Already running
    }

    detector->threadActive = true;
    detector->frameThread = std::thread(frameGenerationThread, detector);
}

void stopFrameThread(MockDetector* detector) {
    detector->threadActive = false;
    detector->acquiring = false;

    if (detector->frameThread.joinable()) {
        detector->frameThread.join();
    }
}

void notifyStateChange(MockDetector* detector, VarexState newState) {
    detector->state = newState;
    if (detector->stateCallback) {
        detector->stateCallback(newState, detector->userContext);
    }
}

} // anonymous namespace

//=============================================================================
// Varex SDK API Implementation
//=============================================================================

extern "C" {

VAREX_API VarexError Varex_Initialize() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (g_sdkInitialized.load()) {
        return VAREX_ERR_ALREADY_INITIALIZED;
    }

    g_sdkInitialized = true;
    return VAREX_OK;
}

VAREX_API VarexError Varex_Shutdown() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    // Cleanup all detectors
    std::lock_guard<std::mutex> detectorsLock(g_detectorsMutex);
    for (auto* detector : g_detectors) {
        stopFrameThread(detector);
        delete detector;
    }
    g_detectors.clear();

    g_sdkInitialized = false;
    return VAREX_OK;
}

VAREX_API VarexError Varex_CreateDetector(const char* config, VarexHandle* outHandle) {
    (void)config; // Config not used in mock

    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!outHandle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = new MockDetector();
    detector->info = createMockDetectorInfo();

    {
        std::lock_guard<std::mutex> lock(g_detectorsMutex);
        g_detectors.push_back(detector);
    }

    *outHandle = detector;
    return VAREX_OK;
}

VAREX_API VarexError Varex_DestroyDetector(VarexHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Stop any ongoing acquisition
    if (detector->acquiring) {
        Varex_StopAcquisition(handle);
    }

    // Remove from global list
    {
        std::lock_guard<std::mutex> lock(g_detectorsMutex);
        auto it = std::find(g_detectors.begin(), g_detectors.end(), detector);
        if (it != g_detectors.end()) {
            g_detectors.erase(it);
        }
    }

    delete detector;
    return VAREX_OK;
}

VAREX_API VarexError Varex_InitializeDetector(VarexHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (detector->initialized) {
        return VAREX_ERR_ALREADY_INITIALIZED;
    }

    detector->initialized = true;
    notifyStateChange(detector, VAREX_STATE_READY);

    return VAREX_OK;
}

VAREX_API VarexError Varex_ShutdownDetector(VarexHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    // Stop acquisition if active
    if (detector->acquiring) {
        Varex_StopAcquisition(handle);
    }

    detector->initialized = false;
    notifyStateChange(detector, VAREX_STATE_IDLE);

    return VAREX_OK;
}

VAREX_API VarexError Varex_GetDetectorInfo(VarexHandle handle, VarexDetectorInfo* outInfo) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outInfo) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outInfo = detector->info;

    return VAREX_OK;
}

VAREX_API VarexError Varex_GetState(VarexHandle handle, VarexState* outState) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outState) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outState = detector->state;

    return VAREX_OK;
}

VAREX_API VarexError Varex_SetAcquisitionParams(VarexHandle handle, const VarexAcqParams* params) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle || !params) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Validate parameters
    if (params->width == 0 || params->height == 0) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    if (params->width > detector->info.maxWidth || params->height > detector->info.maxHeight) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    if (params->exposureTimeMs <= 0) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    if (params->binning != 1 && params->binning != 2 && params->binning != 4) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    detector->params = *params;
    return VAREX_OK;
}

VAREX_API VarexError Varex_GetAcquisitionParams(VarexHandle handle, VarexAcqParams* outParams) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outParams) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outParams = detector->params;

    return VAREX_OK;
}

VAREX_API VarexError Varex_RegisterCallbacks(
    VarexHandle handle,
    VarexImageCallback imageCallback,
    VarexStateCallback stateCallback,
    VarexErrorCallback errorCallback,
    void* userContext
) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    detector->imageCallback = imageCallback;
    detector->stateCallback = stateCallback;
    detector->errorCallback = errorCallback;
    detector->userContext = userContext;

    return VAREX_OK;
}

VAREX_API VarexError Varex_StartAcquisition(VarexHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (detector->acquiring) {
        return VAREX_ERR_STATE_ERROR;
    }

    if (detector->state != VAREX_STATE_READY) {
        return VAREX_ERR_STATE_ERROR;
    }

    detector->acquiring = true;
    notifyStateChange(detector, VAREX_STATE_ACQUIRING);

    // Start frame generation thread
    startFrameThread(detector);

    return VAREX_OK;
}

VAREX_API VarexError Varex_StopAcquisition(VarexHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->acquiring) {
        return VAREX_ERR_STATE_ERROR;
    }

    detector->acquiring = false;
    stopFrameThread(detector);

    notifyStateChange(detector, VAREX_STATE_READY);

    return VAREX_OK;
}

VAREX_API VarexError Varex_IsAcquiring(VarexHandle handle, int* outAcquiring) {
    if (!g_sdkInitialized.load()) {
        return VAREX_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outAcquiring) {
        return VAREX_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outAcquiring = detector->acquiring ? 1 : 0;

    return VAREX_OK;
}

VAREX_API const char* Varex_ErrorToString(VarexError error) {
    switch (error) {
        case VAREX_OK: return "OK";
        case VAREX_ERR_NOT_INITIALIZED: return "Not initialized";
        case VAREX_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case VAREX_ERR_INVALID_PARAMETER: return "Invalid parameter";
        case VAREX_ERR_TIMEOUT: return "Timeout";
        case VAREX_ERR_HARDWARE: return "Hardware error";
        case VAREX_ERR_COMMUNICATION: return "Communication error";
        case VAREX_ERR_NOT_SUPPORTED: return "Not supported";
        case VAREX_ERR_STATE_ERROR: return "State error";
        case VAREX_ERR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

VAREX_API const char* Varex_StateToString(VarexState state) {
    switch (state) {
        case VAREX_STATE_IDLE: return "IDLE";
        case VAREX_STATE_READY: return "READY";
        case VAREX_STATE_ACQUIRING: return "ACQUIRING";
        case VAREX_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // extern "C"
