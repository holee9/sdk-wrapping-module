#include "abyz_sdk.h"
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>

//=============================================================================
// ABYZ Mock SDK Implementation
//=============================================================================

namespace {

// Mock SDK state
static std::atomic<bool> g_sdkInitialized{false};
static std::mutex g_sdkMutex;

// Mock detector data
struct MockDetector {
    bool initialized = false;
    AbyzState state = ABYZ_STATE_IDLE;
    AbyzVendor vendor = ABYZ_VENDOR_RAYENCE;
    AbyzAcqParams params{2048, 2048, 0, 0, 100.0f, 1.0f, 1};
    AbyzDetectorInfo info{};
    uint64_t frameCounter = 0;
    bool acquiring = false;

    // Callbacks
    AbyzImageCallback imageCallback = nullptr;
    AbyzStateCallback stateCallback = nullptr;
    AbyzErrorCallback errorCallback = nullptr;
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

AbyzDetectorInfo createMockDetectorInfo(AbyzVendor vendor) {
    AbyzDetectorInfo info{};
    info.vendor = vendor;

    const char* vendorName = Abyz_VendorToString(vendor);

    switch (vendor) {
        case ABYZ_VENDOR_RAYENCE:
            std::strncpy(info.vendorName, vendorName, sizeof(info.vendorName) - 1);
            std::strncpy(info.model, "Raynex-1417", sizeof(info.model) - 1);
            std::strncpy(info.serialNumber, "RAYENCE-MOCK-001", sizeof(info.serialNumber) - 1);
            std::strncpy(info.firmwareVersion, "3.2.1", sizeof(info.firmwareVersion) - 1);
            info.maxWidth = 2880;
            info.maxHeight = 2880;
            info.bitDepth = 16;
            break;

        case ABYZ_VENDOR_SAMSUNG:
            std::strncpy(info.vendorName, vendorName, sizeof(info.vendorName) - 1);
            std::strncpy(info.model, "X-Ray-170", sizeof(info.model) - 1);
            std::strncpy(info.serialNumber, "SAMSUNG-MOCK-001", sizeof(info.serialNumber) - 1);
            std::strncpy(info.firmwareVersion, "2.0.5", sizeof(info.firmwareVersion) - 1);
            info.maxWidth = 3392;
            info.maxHeight = 3392;
            info.bitDepth = 16;
            break;

        case ABYZ_VENDOR_DRTECH:
            std::strncpy(info.vendorName, vendorName, sizeof(info.vendorName) - 1);
            std::strncpy(info.model, "DRC-101", sizeof(info.model) - 1);
            std::strncpy(info.serialNumber, "DRTECH-MOCK-001", sizeof(info.serialNumber) - 1);
            std::strncpy(info.firmwareVersion, "1.8.0", sizeof(info.firmwareVersion) - 1);
            info.maxWidth = 3072;
            info.maxHeight = 2304;
            info.bitDepth = 16;
            break;

        default:
            std::strncpy(info.vendorName, "Unknown", sizeof(info.vendorName) - 1);
            std::strncpy(info.model, "Unknown", sizeof(info.model) - 1);
            std::strncpy(info.serialNumber, "UNKNOWN-MOCK", sizeof(info.serialNumber) - 1);
            std::strncpy(info.firmwareVersion, "0.0.0", sizeof(info.firmwareVersion) - 1);
            info.maxWidth = 2048;
            info.maxHeight = 2048;
            info.bitDepth = 16;
            break;
    }

    return info;
}

AbyzVendor parseVendorFromConfig(const char* config) {
    if (!config) {
        return ABYZ_VENDOR_RAYENCE; // Default vendor
    }

    std::string configStr(config);
    std::transform(configStr.begin(), configStr.end(), configStr.begin(), ::tolower);

    // Simple JSON parsing for "vendor" field
    size_t vendorPos = configStr.find("\"vendor\"");
    if (vendorPos == std::string::npos) {
        return ABYZ_VENDOR_RAYENCE; // Default vendor
    }

    size_t colonPos = configStr.find(':', vendorPos);
    if (colonPos == std::string::npos) {
        return ABYZ_VENDOR_RAYENCE;
    }

    size_t valueStart = configStr.find('"', colonPos);
    if (valueStart == std::string::npos) {
        return ABYZ_VENDOR_RAYENCE;
    }
    valueStart++; // Skip opening quote

    size_t valueEnd = configStr.find('"', valueStart);
    if (valueEnd == std::string::npos) {
        return ABYZ_VENDOR_RAYENCE;
    }

    std::string vendorValue = configStr.substr(valueStart, valueEnd - valueStart);

    if (vendorValue == "rayence") {
        return ABYZ_VENDOR_RAYENCE;
    } else if (vendorValue == "samsung") {
        return ABYZ_VENDOR_SAMSUNG;
    } else if (vendorValue == "drtech") {
        return ABYZ_VENDOR_DRTECH;
    }

    return ABYZ_VENDOR_RAYENCE; // Default vendor
}

void frameGenerationThread(MockDetector* detector) {
    while (detector->threadActive.load() && detector->acquiring) {
        // Simulate ~25ms frame interval (~40 fps)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        if (!detector->threadActive.load() || !detector->acquiring) {
            break;
        }

        // Prepare frame data (SDK-owned buffer)
        const size_t pixelCount = detector->params.width * detector->params.height;
        if (g_frameBuffer.size() < pixelCount) {
            g_frameBuffer.resize(pixelCount);
        }

        // Generate vendor-specific mock frame patterns
        for (uint32_t y = 0; y < detector->params.height; ++y) {
            for (uint32_t x = 0; x < detector->params.width; ++x) {
                size_t idx = y * detector->params.width + x;
                uint16_t value = 0;

                switch (detector->vendor) {
                    case ABYZ_VENDOR_RAYENCE:
                        // Diagonal gradient pattern
                        value = static_cast<uint16_t>(((x + y) * 65535) / (detector->params.width + detector->params.height));
                        break;

                    case ABYZ_VENDOR_SAMSUNG:
                        // Radial gradient pattern
                        {
                            float cx = detector->params.width / 2.0f;
                            float cy = detector->params.height / 2.0f;
                            float dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                            float maxDist = std::sqrt(cx * cx + cy * cy);
                            value = static_cast<uint16_t>((dist / maxDist) * 65535);
                        }
                        break;

                    case ABYZ_VENDOR_DRTECH:
                        // Horizontal stripes pattern
                        {
                            uint32_t stripeWidth = 32;
                            uint32_t stripe = (y / stripeWidth) % 2;
                            value = stripe ? 50000 : 15000;
                        }
                        break;

                    default:
                        value = 32768; // Middle gray
                        break;
                }

                // Add frame counter variation
                value = static_cast<uint16_t>((value + detector->frameCounter * 50) % 65536);
                g_frameBuffer[idx] = value;
            }
        }

        // Create image structure (SDK-owned memory)
        AbyzImage image{};
        image.data = g_frameBuffer.data();
        image.width = detector->params.width;
        image.height = detector->params.height;
        image.bitDepth = 16;
        image.frameNumber = ++detector->frameCounter;
        image.timestamp = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        image.dataLength = static_cast<uint32_t>(pixelCount * sizeof(uint16_t));
        image.vendor = detector->vendor;

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

void notifyStateChange(MockDetector* detector, AbyzState newState) {
    detector->state = newState;
    if (detector->stateCallback) {
        detector->stateCallback(newState, detector->userContext);
    }
}

} // anonymous namespace

//=============================================================================
// ABYZ SDK API Implementation
//=============================================================================

extern "C" {

ABYZ_API AbyzError Abyz_Initialize() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (g_sdkInitialized.load()) {
        return ABYZ_ERR_ALREADY_INITIALIZED;
    }

    g_sdkInitialized = true;
    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_Shutdown() {
    std::lock_guard<std::mutex> lock(g_sdkMutex);

    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    // Cleanup all detectors
    std::lock_guard<std::mutex> detectorsLock(g_detectorsMutex);
    for (auto* detector : g_detectors) {
        stopFrameThread(detector);
        delete detector;
    }
    g_detectors.clear();

    g_sdkInitialized = false;
    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_CreateDetector(const char* config, AbyzHandle* outHandle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!outHandle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    // Parse vendor from config
    AbyzVendor vendor = parseVendorFromConfig(config);

    auto* detector = new MockDetector();
    detector->vendor = vendor;
    detector->info = createMockDetectorInfo(vendor);

    {
        std::lock_guard<std::mutex> lock(g_detectorsMutex);
        g_detectors.push_back(detector);
    }

    *outHandle = detector;
    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_DestroyDetector(AbyzHandle handle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Stop any ongoing acquisition
    if (detector->acquiring) {
        Abyz_StopAcquisition(handle);
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
    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_InitializeDetector(AbyzHandle handle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (detector->initialized) {
        return ABYZ_ERR_ALREADY_INITIALIZED;
    }

    detector->initialized = true;
    notifyStateChange(detector, ABYZ_STATE_READY);

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_ShutdownDetector(AbyzHandle handle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    // Stop acquisition if active
    if (detector->acquiring) {
        Abyz_StopAcquisition(handle);
    }

    detector->initialized = false;
    notifyStateChange(detector, ABYZ_STATE_IDLE);

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_GetDetectorInfo(AbyzHandle handle, AbyzDetectorInfo* outInfo) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outInfo) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outInfo = detector->info;

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_GetState(AbyzHandle handle, AbyzState* outState) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outState) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outState = detector->state;

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_SetAcquisitionParams(AbyzHandle handle, const AbyzAcqParams* params) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle || !params) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    // Validate parameters
    if (params->width == 0 || params->height == 0) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    if (params->width > detector->info.maxWidth || params->height > detector->info.maxHeight) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    if (params->exposureTimeMs <= 0) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    if (params->binning != 1 && params->binning != 2 && params->binning != 4) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    detector->params = *params;
    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_GetAcquisitionParams(AbyzHandle handle, AbyzAcqParams* outParams) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outParams) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outParams = detector->params;

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_RegisterCallbacks(
    AbyzHandle handle,
    AbyzImageCallback imageCallback,
    AbyzStateCallback stateCallback,
    AbyzErrorCallback errorCallback,
    void* userContext
) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    detector->imageCallback = imageCallback;
    detector->stateCallback = stateCallback;
    detector->errorCallback = errorCallback;
    detector->userContext = userContext;

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_StartAcquisition(AbyzHandle handle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->initialized) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (detector->acquiring) {
        return ABYZ_ERR_STATE_ERROR;
    }

    if (detector->state != ABYZ_STATE_READY) {
        return ABYZ_ERR_STATE_ERROR;
    }

    detector->acquiring = true;
    notifyStateChange(detector, ABYZ_STATE_ACQUIRING);

    // Start frame generation thread
    startFrameThread(detector);

    return ABYZ_OK;
}

ABYZ_API AbyzError Abyz_StopAcquisition(AbyzHandle handle) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);

    if (!detector->acquiring) {
        return ABYZ_ERR_STATE_ERROR;
    }

    detector->acquiring = false;
    stopFrameThread(detector);

    notifyStateChange(detector, ABYZ_STATE_READY);

    return ABYZ_OK;
}

ABYZ_API enum AbyzError Abyz_IsAcquiring(AbyzHandle handle, int* outAcquiring) {
    if (!g_sdkInitialized.load()) {
        return ABYZ_ERR_NOT_INITIALIZED;
    }

    if (!handle || !outAcquiring) {
        return ABYZ_ERR_INVALID_PARAMETER;
    }

    auto* detector = static_cast<MockDetector*>(handle);
    *outAcquiring = detector->acquiring ? 1 : 0;

    return ABYZ_OK;
}

ABYZ_API const char* Abyz_ErrorToString(AbyzError error) {
    switch (error) {
        case ABYZ_OK: return "OK";
        case ABYZ_ERR_NOT_INITIALIZED: return "Not initialized";
        case ABYZ_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case ABYZ_ERR_INVALID_PARAMETER: return "Invalid parameter";
        case ABYZ_ERR_TIMEOUT: return "Timeout";
        case ABYZ_ERR_HARDWARE: return "Hardware error";
        case ABYZ_ERR_COMMUNICATION: return "Communication error";
        case ABYZ_ERR_NOT_SUPPORTED: return "Not supported";
        case ABYZ_ERR_STATE_ERROR: return "State error";
        case ABYZ_ERR_OUT_OF_MEMORY: return "Out of memory";
        case ABYZ_ERR_UNKNOWN_VENDOR: return "Unknown vendor";
        default: return "Unknown error";
    }
}

ABYZ_API const char* Abyz_StateToString(AbyzState state) {
    switch (state) {
        case ABYZ_STATE_IDLE: return "IDLE";
        case ABYZ_STATE_READY: return "READY";
        case ABYZ_STATE_ACQUIRING: return "ACQUIRING";
        case ABYZ_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

ABYZ_API const char* Abyz_VendorToString(AbyzVendor vendor) {
    switch (vendor) {
        case ABYZ_VENDOR_RAYENCE: return "Rayence";
        case ABYZ_VENDOR_SAMSUNG: return "Samsung";
        case ABYZ_VENDOR_DRTECH: return "DRTech";
        default: return "Unknown";
    }
}

} // extern "C"
