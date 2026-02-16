#include "vieworks_sdk.h"
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

//=============================================================================
// Vieworks Mock SDK Implementation
//=============================================================================

namespace {

// Mock SDK state
static std::atomic<bool> g_sdkInitialized{false};
static std::mutex g_sdkMutex;

// Mock detector data
struct MockDetector {
    bool initialized = false;
    VieworksState state = VIEWORKS_STATE_STANDBY;
    VieworksAcqParams params{2048, 2048, 0, 0, 100.0f, 1.0f, 1};
    VieworksDetectorInfo info{};
    uint64_t frameCounter = 0;
    bool acquiring = false;
    bool frameReady = false;

    // Frame data buffer (stable until next ReadFrame)
    std::vector<uint16_t> frameBuffer;
    VieworksFrame currentFrame{};
};

static std::vector<MockDetector*> g_detectors;
static std::mutex g_detectorsMutex;

//=============================================================================
// Internal Helper Functions
//=============================================================================

VieworksDetectorInfo createMockDetectorInfo() {
    VieworksDetectorInfo info{};
    std::strncpy(info.vendor, "Vieworks", sizeof(info.vendor) - 1);
    std::strncpy(info.model, "Mock-VIVIX", sizeof(info.model) - 1);
    std::strncpy(info.serialNumber, "VIEWORKS-MOCK-001", sizeof(info.serialNumber) - 1);
    std::strncpy(info.firmwareVersion, "1.5.2", sizeof(info.firmwareVersion) - 1);
    info.maxWidth = 4096;
    info.maxHeight = 4096;
    info.bitDepth = 16;
    return info;
}

void generateMockFrame(MockDetector* detector) {
    const size_t pixelCount = detector->params.width * detector->params.height;

    // Allocate buffer if needed
    if (detector->frameBuffer.size() < pixelCount) {
        detector->frameBuffer.resize(pixelCount);
    }

    // Generate mock frame data (checkerboard pattern)
    for (uint32_t y = 0; y < detector->params.height; ++y) {
        for (uint32_t x = 0; x < detector->params.width; ++x) {
            size_t idx = y * detector->params.width + x;
            // Create checkerboard pattern
            uint32_t tileSize = 64;
            bool white = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            uint16_t baseValue = white ? 50000 : 10000;

            // Add gradient within each tile
            uint32_t tileX = x % tileSize;
            uint32_t tileY = y % tileSize;
            uint16_t variation = static_cast<uint16_t>(
                (tileX * 20000 / tileSize) + (tileY * 20000 / tileSize)
            );

            // Add frame counter variation
            uint16_t frameOffset = static_cast<uint16_t>((detector->frameCounter * 500) % 10000);

            detector->frameBuffer[idx] = baseValue + variation + frameOffset;
        }
    }

    // Update frame structure
    detector->currentFrame.data = detector->frameBuffer.data();
    detector->currentFrame.width = detector->params.width;
    detector->currentFrame.height = detector->params.height;
    detector->currentFrame.bitDepth = 16;
    detector->currentFrame.frameNumber = ++detector->frameCounter;
    detector->currentFrame.timestamp = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    detector->currentFrame.dataLength = static_cast<uint32_t>(pixelCount * sizeof(uint16_t));

    detector->frameReady = true;
}

} // anonymous namespace

//=============================================================================
// Vieworks SDK API Implementation
//=============================================================================

extern "C" {

VIEWORKS_API VieworksStatus Vieworks_Initialize() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (g_sdkInitialized.load()) {
        return VIEWORKS_ERR_ALREADY_INITIALIZED;
    }

    g_sdkInitialized = true;
    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_Shutdown() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    // Cleanup all detectors
    std::lock_guard<std::mutex> detectorsLock(g_detectorsMutex);
    for (auto* detector : g_detectors) {
        delete detector;
    }
    g_detectors.clear();

    g_sdkInitialized = false;
    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_CreateDetector(const char* config, VieworksHandle* outHandle) {
    (void)config; // Config not used in mock

    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!outHandle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = new MockDetector();
    detector->info = createMockDetectorInfo();

    {
        std::lock_guard<std::mutex> lock(g_detectorsMutex);
        g_detectors.push_back(detector);
    }

    *outHandle = detector;
    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_DestroyDetector(VieworksHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Stop any ongoing acquisition
    if (detector->acquiring) {
        Vieworks_StopAcquisition(handle);
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
    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_InitializeDetector(VieworksHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (detector->initialized) {
        return VIEWORKS_ERR_ALREADY_INITIALIZED;
    }

    detector->initialized = true;
    detector->state = VIEWORKS_STATE_READY;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_ShutdownDetector(VieworksHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    // Stop acquisition if active
    if (detector->acquiring) {
        Vieworks_StopAcquisition(handle);
    }

    detector->initialized = false;
    detector->state = VIEWORKS_STATE_STANDBY;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_GetDetectorInfo(VieworksHandle handle, VieworksDetectorInfo* outInfo) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outInfo) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outInfo = detector->info;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_GetState(VieworksHandle handle, VieworksState* outState) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outState) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outState = detector->state;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_SetAcquisitionParams(VieworksHandle handle, const VieworksAcqParams* params) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !params) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Validate parameters
    if (params->width == 0 || params->height == 0) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    if (params->width > detector->info.maxWidth || params->height > detector->info.maxHeight) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    if (params->exposureTimeMs <= 0) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    if (params->binning != 1 && params->binning != 2 && params->binning != 4) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    detector->params = *params;
    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_GetAcquisitionParams(VieworksHandle handle, VieworksAcqParams* outParams) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outParams) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outParams = detector->params;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_StartAcquisition(VieworksHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (detector->acquiring) {
        return VIEWORKS_ERR_STATE_ERROR;
    }

    if (detector->state != VIEWORKS_STATE_READY) {
        return VIEWORKS_ERR_STATE_ERROR;
    }

    detector->acquiring = true;
    detector->frameReady = false;
    detector->state = VIEWORKS_STATE_EXPOSING;

    // Simulate initial frame becoming available after exposure time
    std::thread([detector]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>(detector->params.exposureTimeMs)
        ));
        if (detector->acquiring) {
            generateMockFrame(detector);
            detector->state = VIEWORKS_STATE_READY;
        }
    }).detach();

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_StopAcquisition(VieworksHandle handle) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->acquiring) {
        return VIEWORKS_ERR_STATE_ERROR;
    }

    detector->acquiring = false;
    detector->frameReady = false;
    detector->state = VIEWORKS_STATE_READY;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_GetFrameReady(VieworksHandle handle, int* outReady) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outReady) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    *outReady = detector->frameReady ? 1 : 0;

    // Auto-generate next frame if acquiring and no frame ready
    if (detector->acquiring && !detector->frameReady && detector->state == VIEWORKS_STATE_READY) {
        std::thread([detector]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // ~50fps
            if (detector->acquiring) {
                generateMockFrame(detector);
            }
        }).detach();
    }

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_ReadFrame(VieworksHandle handle, VieworksFrame* outFrame) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outFrame) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->frameReady) {
        return VIEWORKS_ERR_STATE_ERROR;
    }

    detector->state = VIEWORKS_STATE_READING;
    *outFrame = detector->currentFrame;
    detector->frameReady = false;
    detector->state = VIEWORKS_STATE_READY;

    return VIEWORKS_OK;
}

VIEWORKS_API VieworksStatus Vieworks_IsAcquiring(VieworksHandle handle, int* outAcquiring) {
    if (!g_sdkInitialized.load()) {
        return VIEWORKS_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outAcquiring) {
        return VIEWORKS_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outAcquiring = detector->acquiring ? 1 : 0;

    return VIEWORKS_OK;
}

VIEWORKS_API const char* Vieworks_StatusToString(VieworksStatus status) {
    switch (status) {
        case VIEWORKS_OK: return "OK";
        case VIEWORKS_ERR_NOT_INITIALIZED: return "Not initialized";
        case VIEWORKS_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case VIEWORKS_ERR_INVALID_PARAMETER: return "Invalid parameter";
        case VIEWORKS_ERR_TIMEOUT: return "Timeout";
        case VIEWORKS_ERR_HARDWARE: return "Hardware error";
        case VIEWORKS_ERR_COMMUNICATION: return "Communication error";
        case VIEWORKS_ERR_NOT_SUPPORTED: return "Not supported";
        case VIEWORKS_ERR_STATE_ERROR: return "State error";
        case VIEWORKS_ERR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

VIEWORKS_API const char* Vieworks_StateToString(VieworksState state) {
    switch (state) {
        case VIEWORKS_STATE_STANDBY: return "STANDBY";
        case VIEWORKS_STATE_READY: return "READY";
        case VIEWORKS_STATE_EXPOSING: return "EXPOSING";
        case VIEWORKS_STATE_READING: return "READING";
        case VIEWORKS_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // extern "C"
