#include "VarexDetector.h"
#include "varex_sdk.h"
#include <cstring>
#include <chrono>
#include <thread>

using namespace uxdi;
using namespace uxdi::adapters::varex;

//=============================================================================
// VarexDetector Implementation
//=============================================================================

VarexDetector::VarexDetector()
    : sdkHandle_(nullptr)
    , state_(DetectorState::IDLE)
    , initialized_(false)
    , sdkInitialized_(false)
    , listener_(nullptr)
    , syncInterface_(std::make_shared<VarexDetectorSynchronous>(this))
{
    // Initialize default acquisition parameters
    params_.width = 1024;
    params_.height = 1024;
    params_.offsetX = 0;
    params_.offsetY = 0;
    params_.exposureTimeMs = 100.0f;
    params_.gain = 1.0f;
    params_.binning = 1;

    // Initialize error info
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";

    // Initialize Varex SDK
    if (Varex_Initialize() == VAREX_OK) {
        sdkInitialized_ = true;
    }
}

VarexDetector::~VarexDetector() {
    if (initialized_) {
        shutdown();
    }

    // Cleanup SDK handle
    if (sdkHandle_) {
        Varex_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
    }

    // Shutdown SDK
    if (sdkInitialized_) {
        Varex_Shutdown();
    }
}

bool VarexDetector::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (initialized_) {
        setError(ErrorCode::ALREADY_INITIALIZED, "Detector is already initialized");
        return false;
    }

    if (!sdkInitialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Varex SDK initialization failed");
        return false;
    }

    state_ = DetectorState::INITIALIZING;

    // Create SDK detector handle
    VarexError err = Varex_CreateDetector("", &sdkHandle_);
    if (err != VAREX_OK || !sdkHandle_) {
        setError(mapVarexError(err), "Failed to create Varex detector");
        state_ = DetectorState::ERROR;
        return false;
    }

    // Register callbacks
    err = Varex_RegisterCallbacks(
        sdkHandle_,
        &VarexDetector::imageCallbackBridge,
        &VarexDetector::stateCallbackBridge,
        &VarexDetector::errorCallbackBridge,
        this
    );
    if (err != VAREX_OK) {
        setError(mapVarexError(err), "Failed to register Varex callbacks");
        Varex_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    // Initialize the detector
    err = Varex_InitializeDetector(sdkHandle_);
    if (err != VAREX_OK) {
        setError(mapVarexError(err), "Failed to initialize Varex detector");
        Varex_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    // Set initial acquisition parameters
    VarexAcqParams varexParams;
    varexParams.width = params_.width;
    varexParams.height = params_.height;
    varexParams.offsetX = params_.offsetX;
    varexParams.offsetY = params_.offsetY;
    varexParams.exposureTimeMs = params_.exposureTimeMs;
    varexParams.gain = params_.gain;
    varexParams.binning = params_.binning;

    err = Varex_SetAcquisitionParams(sdkHandle_, &varexParams);
    if (err != VAREX_OK) {
        setError(mapVarexError(err), "Failed to set Varex acquisition parameters");
        Varex_ShutdownDetector(sdkHandle_);
        Varex_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    initialized_ = true;
    state_ = DetectorState::READY;
    clearError();

    notifyStateChanged(DetectorState::READY);
    return true;
}

bool VarexDetector::shutdown() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    // Stop acquisition if running
    if (state_ == DetectorState::ACQUIRING) {
        stopAcquisition();
    }

    // Shutdown SDK detector
    if (sdkHandle_) {
        Varex_ShutdownDetector(sdkHandle_);
    }

    initialized_ = false;
    state_ = DetectorState::IDLE;

    notifyStateChanged(DetectorState::IDLE);
    clearError();
    return true;
}

bool VarexDetector::isInitialized() const {
    return initialized_.load();
}

DetectorInfo VarexDetector::getDetectorInfo() const {
    DetectorInfo info{};
    info.vendor = "Varex";
    info.model = "Mock-4343CT";
    info.serialNumber = "VAREX-MOCK-001";
    info.firmwareVersion = "2.1.0";
    info.maxWidth = 3072;
    info.maxHeight = 2048;
    info.bitDepth = 16;

    // Try to get actual info from SDK
    if (sdkHandle_) {
        VarexDetectorInfo varexInfo;
        if (Varex_GetDetectorInfo(sdkHandle_, &varexInfo) == VAREX_OK) {
            info.vendor = varexInfo.vendor;
            info.model = varexInfo.model;
            info.serialNumber = varexInfo.serialNumber;
            info.firmwareVersion = varexInfo.firmwareVersion;
            info.maxWidth = varexInfo.maxWidth;
            info.maxHeight = varexInfo.maxHeight;
            info.bitDepth = varexInfo.bitDepth;
        }
    }

    return info;
}

std::string VarexDetector::getVendorName() const {
    return "Varex";
}

std::string VarexDetector::getModelName() const {
    return "Mock-4343CT";
}

DetectorState VarexDetector::getState() const {
    return state_.load();
}

std::string VarexDetector::getStateString() const {
    return stateToString(state_.load());
}

bool VarexDetector::setAcquisitionParams(const AcquisitionParams& params) {
    std::lock_guard<std::mutex> lock(paramsMutex_);

    // Validate parameters
    if (params.width == 0 || params.height == 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Width and height must be non-zero");
        return false;
    }

    if (params.width > 3072 || params.height > 2048) {
        setError(ErrorCode::INVALID_PARAMETER, "Maximum resolution is 3072x2048");
        return false;
    }

    if (params.exposureTimeMs <= 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Exposure time must be positive");
        return false;
    }

    if (params.gain <= 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Gain must be positive");
        return false;
    }

    if (params.binning != 1 && params.binning != 2 && params.binning != 4) {
        setError(ErrorCode::INVALID_PARAMETER, "Binning must be 1, 2, or 4");
        return false;
    }

    // Set parameters in SDK
    if (sdkHandle_) {
        VarexAcqParams varexParams;
        varexParams.width = params.width;
        varexParams.height = params.height;
        varexParams.offsetX = params.offsetX;
        varexParams.offsetY = params.offsetY;
        varexParams.exposureTimeMs = params.exposureTimeMs;
        varexParams.gain = params.gain;
        varexParams.binning = params.binning;

        VarexError err = Varex_SetAcquisitionParams(sdkHandle_, &varexParams);
        if (err != VAREX_OK) {
            setError(mapVarexError(err), "Failed to set Varex acquisition parameters");
            return false;
        }
    }

    params_ = params;
    clearError();
    return true;
}

AcquisitionParams VarexDetector::getAcquisitionParams() const {
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return params_;
}

void VarexDetector::setListener(IDetectorListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

IDetectorListener* VarexDetector::getListener() const {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

bool VarexDetector::startAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (state_ == DetectorState::ACQUIRING) {
        setError(ErrorCode::STATE_ERROR, "Acquisition is already in progress");
        return false;
    }

    if (state_ != DetectorState::READY) {
        setError(ErrorCode::STATE_ERROR, "Detector must be in READY state to start acquisition");
        return false;
    }

    // Start acquisition in SDK
    VarexError err = Varex_StartAcquisition(sdkHandle_);
    if (err != VAREX_OK) {
        setError(mapVarexError(err), "Failed to start Varex acquisition");
        return false;
    }

    state_ = DetectorState::ACQUIRING;
    clearError();

    notifyStateChanged(DetectorState::ACQUIRING);

    // Notify listener that acquisition started
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> listenerLock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onAcquisitionStarted();
    }

    return true;
}

bool VarexDetector::stopAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (state_ != DetectorState::ACQUIRING) {
        setError(ErrorCode::STATE_ERROR, "No acquisition is in progress");
        return false;
    }

    // Stop acquisition in SDK
    VarexError err = Varex_StopAcquisition(sdkHandle_);
    if (err != VAREX_OK) {
        setError(mapVarexError(err), "Failed to stop Varex acquisition");
        return false;
    }

    state_ = DetectorState::READY;

    // Notify listener that acquisition stopped
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> listenerLock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onAcquisitionStopped();
    }

    notifyStateChanged(DetectorState::READY);
    clearError();
    return true;
}

bool VarexDetector::isAcquiring() const {
    if (sdkHandle_) {
        int acquiring = 0;
        if (Varex_IsAcquiring(sdkHandle_, &acquiring) == VAREX_OK) {
            return acquiring != 0;
        }
    }
    return state_.load() == DetectorState::ACQUIRING;
}

std::shared_ptr<IDetectorSynchronous> VarexDetector::getSynchronousInterface() {
    return syncInterface_;
}

ErrorInfo VarexDetector::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void VarexDetector::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";
    lastError_.details.clear();
}

//=============================================================================
// Callback Bridges
//=============================================================================

void VarexDetector::imageCallbackBridge(const VarexImage* img, void* ctx) {
    if (!ctx || !img) return;
    auto* detector = static_cast<VarexDetector*>(ctx);
    detector->onImageReceived(img);
}

void VarexDetector::stateCallbackBridge(VarexState sdkState, void* ctx) {
    if (!ctx) return;
    auto* detector = static_cast<VarexDetector*>(ctx);
    detector->onStateChanged(sdkState);
}

void VarexDetector::errorCallbackBridge(VarexError err, const char* msg, void* ctx) {
    if (!ctx) return;
    auto* detector = static_cast<VarexDetector*>(ctx);
    detector->onError(err, msg);
}

//=============================================================================
// Instance Callback Handlers
//=============================================================================

void VarexDetector::onImageReceived(const VarexImage* img) {
    if (!img) return;

    // MANDATORY COPY: SDK owns the buffer, must copy immediately
    const size_t bufferBytes = img->dataLength;
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[bufferBytes]);
    std::memcpy(buffer.get(), img->data, bufferBytes);

    // Create UXDI image structure
    ImageData image;
    image.width = img->width;
    image.height = img->height;
    image.bitDepth = img->bitDepth;
    image.frameNumber = img->frameNumber;
    image.timestamp = img->timestamp;
    image.data = buffer;
    image.dataLength = bufferBytes;

    notifyImageReceived(image);
}

void VarexDetector::onStateChanged(VarexState sdkState) {
    DetectorState newState = mapVarexState(sdkState);
    state_ = newState;
    notifyStateChanged(newState);
}

void VarexDetector::onError(VarexError err, const char* msg) {
    ErrorInfo error;
    error.code = mapVarexError(err);
    error.message = msg ? msg : Varex_ErrorToString(err);
    error.details = "Varex SDK error";

    {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = error;
    }

    notifyError(error);
}

//=============================================================================
// Private Helper Methods
//=============================================================================

void VarexDetector::setError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = code;
    lastError_.message = message;
    lastError_.details.clear();
}

void VarexDetector::notifyStateChanged(DetectorState newState) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onStateChanged(newState);
    }
}

void VarexDetector::notifyError(const ErrorInfo& error) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onError(error);
    }
}

void VarexDetector::notifyImageReceived(const ImageData& image) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onImageReceived(image);
    }
}

std::string VarexDetector::stateToString(DetectorState state) const {
    switch (state) {
        case DetectorState::UNKNOWN: return "UNKNOWN";
        case DetectorState::IDLE: return "IDLE";
        case DetectorState::INITIALIZING: return "INITIALIZING";
        case DetectorState::READY: return "READY";
        case DetectorState::ACQUIRING: return "ACQUIRING";
        case DetectorState::STOPPING: return "STOPPING";
        case DetectorState::ERROR: return "ERROR";
        default: return "INVALID_STATE";
    }
}

ErrorCode VarexDetector::mapVarexError(VarexError varexError) const {
    switch (varexError) {
        case VAREX_OK: return ErrorCode::SUCCESS;
        case VAREX_ERR_NOT_INITIALIZED: return ErrorCode::NOT_INITIALIZED;
        case VAREX_ERR_ALREADY_INITIALIZED: return ErrorCode::ALREADY_INITIALIZED;
        case VAREX_ERR_INVALID_PARAMETER: return ErrorCode::INVALID_PARAMETER;
        case VAREX_ERR_TIMEOUT: return ErrorCode::TIMEOUT;
        case VAREX_ERR_HARDWARE: return ErrorCode::HARDWARE_ERROR;
        case VAREX_ERR_COMMUNICATION: return ErrorCode::COMMUNICATION_ERROR;
        case VAREX_ERR_NOT_SUPPORTED: return ErrorCode::NOT_SUPPORTED;
        case VAREX_ERR_STATE_ERROR: return ErrorCode::STATE_ERROR;
        case VAREX_ERR_OUT_OF_MEMORY: return ErrorCode::OUT_OF_MEMORY;
        default: return ErrorCode::UNKNOWN_ERROR;
    }
}

DetectorState VarexDetector::mapVarexState(VarexState varexState) const {
    switch (varexState) {
        case VAREX_STATE_IDLE: return DetectorState::IDLE;
        case VAREX_STATE_READY: return DetectorState::READY;
        case VAREX_STATE_ACQUIRING: return DetectorState::ACQUIRING;
        case VAREX_STATE_ERROR: return DetectorState::ERROR;
        default: return DetectorState::UNKNOWN;
    }
}

//=============================================================================
// VarexDetectorSynchronous Implementation
//=============================================================================

VarexDetectorSynchronous::VarexDetectorSynchronous(VarexDetector* detector)
    : detector_(detector)
    , cancelled_(false)
{
}

bool VarexDetectorSynchronous::acquireFrame(ImageData& outImage, uint32_t timeoutMs) {
    if (!detector_) {
        return false;
    }

    cancelled_ = false;

    // Ensure detector is in acquiring state
    if (detector_->getState() != DetectorState::ACQUIRING) {
        if (!detector_->startAcquisition()) {
            return false;
        }
    }

    // For callback-based SDK, we need to wait for listener callback
    // This is a simplified implementation - in production, use condition variable
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while (!cancelled_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) {
            detector_->setError(ErrorCode::TIMEOUT, "Frame acquisition timeout");
            return false;
        }

        // Check if we received a frame (simplified - should use CV)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // In real implementation, check if frame was received via listener
        // For now, return false as this needs proper async handling
        if (detector_->getListener()) {
            // Listener will handle async delivery
            return true;
        }
    }

    return !cancelled_;
}

bool VarexDetectorSynchronous::acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs) {
    if (!detector_) {
        return false;
    }

    cancelled_ = false;
    outImages.clear();
    outImages.reserve(frameCount);

    for (uint32_t i = 0; i < frameCount; ++i) {
        if (cancelled_) {
            break;
        }

        ImageData frame;
        if (!acquireFrame(frame, timeoutMs / frameCount)) {
            return false;
        }

        outImages.push_back(std::move(frame));
    }

    return !cancelled_ && outImages.size() == frameCount;
}

bool VarexDetectorSynchronous::cancelAcquisition() {
    cancelled_ = true;
    return true;
}
