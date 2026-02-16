#include "ABYZDetector.h"
#include "abyz_sdk.h"
#include <cstring>
#include <chrono>
#include <thread>

using namespace uxdi;
using namespace uxdi::adapters::abyz;

//=============================================================================
// ABYZDetector Implementation
//=============================================================================

ABYZDetector::ABYZDetector(const std::string& config)
    : config_(config)
    , sdkHandle_(nullptr)
    , state_(DetectorState::IDLE)
    , initialized_(false)
    , sdkInitialized_(false)
    , listener_(nullptr)
    , syncInterface_(std::make_shared<ABYZDetectorSynchronous>(this))
{
    // Initialize default acquisition parameters
    params_.width = 2048;
    params_.height = 2048;
    params_.offsetX = 0;
    params_.offsetY = 0;
    params_.exposureTimeMs = 100.0f;
    params_.gain = 1.0f;
    params_.binning = 1;

    // Initialize error info
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";

    // Initialize vendor info
    std::memset(&vendorInfo_, 0, sizeof(vendorInfo_));

    // Initialize ABYZ SDK
    if (Abyz_Initialize() == ABYZ_OK) {
        sdkInitialized_ = true;
    }
}

ABYZDetector::~ABYZDetector() {
    if (initialized_) {
        shutdown();
    }

    // Cleanup SDK handle
    if (sdkHandle_) {
        Abyz_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
    }

    // Shutdown SDK
    if (sdkInitialized_) {
        Abyz_Shutdown();
    }
}

bool ABYZDetector::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (initialized_) {
        setError(ErrorCode::ALREADY_INITIALIZED, "Detector is already initialized");
        return false;
    }

    if (!sdkInitialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "ABYZ SDK initialization failed");
        return false;
    }

    state_ = DetectorState::INITIALIZING;

    // Create SDK detector handle with config
    AbyzError err = Abyz_CreateDetector(config_.c_str(), &sdkHandle_);
    if (err != ABYZ_OK || !sdkHandle_) {
        setError(mapAbyzError(err), "Failed to create ABYZ detector");
        state_ = DetectorState::ERROR;
        return false;
    }

    // Register callbacks
    err = Abyz_RegisterCallbacks(
        sdkHandle_,
        &ABYZDetector::imageCallbackBridge,
        &ABYZDetector::stateCallbackBridge,
        &ABYZDetector::errorCallbackBridge,
        this
    );
    if (err != ABYZ_OK) {
        setError(mapAbyzError(err), "Failed to register ABYZ callbacks");
        Abyz_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    // Initialize the detector
    err = Abyz_InitializeDetector(sdkHandle_);
    if (err != ABYZ_OK) {
        setError(mapAbyzError(err), "Failed to initialize ABYZ detector");
        Abyz_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    // Get vendor info
    err = Abyz_GetDetectorInfo(sdkHandle_, &vendorInfo_);
    if (err != ABYZ_OK) {
        // Continue anyway, will use defaults
    }

    // Set initial acquisition parameters
    AbyzAcqParams abyzParams;
    abyzParams.width = params_.width;
    abyzParams.height = params_.height;
    abyzParams.offsetX = params_.offsetX;
    abyzParams.offsetY = params_.offsetY;
    abyzParams.exposureTimeMs = params_.exposureTimeMs;
    abyzParams.gain = params_.gain;
    abyzParams.binning = params_.binning;

    err = Abyz_SetAcquisitionParams(sdkHandle_, &abyzParams);
    if (err != ABYZ_OK) {
        setError(mapAbyzError(err), "Failed to set ABYZ acquisition parameters");
        Abyz_ShutdownDetector(sdkHandle_);
        Abyz_DestroyDetector(sdkHandle_);
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

bool ABYZDetector::shutdown() {
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
        Abyz_ShutdownDetector(sdkHandle_);
    }

    initialized_ = false;
    state_ = DetectorState::IDLE;

    notifyStateChanged(DetectorState::IDLE);
    clearError();
    return true;
}

bool ABYZDetector::isInitialized() const {
    return initialized_.load();
}

DetectorInfo ABYZDetector::getDetectorInfo() const {
    DetectorInfo info{};

    // Get info from vendor info
    info.vendor = Abyz_VendorToString(vendorInfo_.vendor);
    info.model = vendorInfo_.model;
    info.serialNumber = vendorInfo_.serialNumber;
    info.firmwareVersion = vendorInfo_.firmwareVersion;
    info.maxWidth = vendorInfo_.maxWidth;
    info.maxHeight = vendorInfo_.maxHeight;
    info.bitDepth = vendorInfo_.bitDepth;

    // If no info available, try to get it from SDK
    if (info.vendor.empty() && sdkHandle_) {
        AbyzDetectorInfo abyzInfo;
        if (Abyz_GetDetectorInfo(sdkHandle_, &abyzInfo) == ABYZ_OK) {
            info.vendor = Abyz_VendorToString(abyzInfo.vendor);
            info.model = abyzInfo.model;
            info.serialNumber = abyzInfo.serialNumber;
            info.firmwareVersion = abyzInfo.firmwareVersion;
            info.maxWidth = abyzInfo.maxWidth;
            info.maxHeight = abyzInfo.maxHeight;
            info.bitDepth = abyzInfo.bitDepth;
        }
    }

    // Fallback defaults if still empty
    if (info.vendor.empty()) {
        info.vendor = "ABYZ";
        info.model = "Multi-Vendor";
        info.serialNumber = "ABYZ-MOCK-001";
        info.firmwareVersion = "1.0.0";
        info.maxWidth = 3392;
        info.maxHeight = 3392;
        info.bitDepth = 16;
    }

    return info;
}

std::string ABYZDetector::getVendorName() const {
    if (vendorInfo_.vendorName[0] != '\0') {
        return vendorInfo_.vendorName;
    }
    return Abyz_VendorToString(vendorInfo_.vendor);
}

std::string ABYZDetector::getModelName() const {
    if (vendorInfo_.model[0] != '\0') {
        return vendorInfo_.model;
    }
    return "Multi-Vendor-Detector";
}

DetectorState ABYZDetector::getState() const {
    return state_.load();
}

std::string ABYZDetector::getStateString() const {
    return stateToString(state_.load());
}

bool ABYZDetector::setAcquisitionParams(const AcquisitionParams& params) {
    std::lock_guard<std::mutex> lock(paramsMutex_);

    // Validate parameters
    if (params.width == 0 || params.height == 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Width and height must be non-zero");
        return false;
    }

    // Get max dimensions from vendor info
    uint32_t maxWidth = vendorInfo_.maxWidth > 0 ? vendorInfo_.maxWidth : 3392;
    uint32_t maxHeight = vendorInfo_.maxHeight > 0 ? vendorInfo_.maxHeight : 3392;

    if (params.width > maxWidth || params.height > maxHeight) {
        setError(ErrorCode::INVALID_PARAMETER, "Maximum resolution is " +
                  std::to_string(maxWidth) + "x" + std::to_string(maxHeight));
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
        AbyzAcqParams abyzParams;
        abyzParams.width = params.width;
        abyzParams.height = params.height;
        abyzParams.offsetX = params.offsetX;
        abyzParams.offsetY = params.offsetY;
        abyzParams.exposureTimeMs = params.exposureTimeMs;
        abyzParams.gain = params.gain;
        abyzParams.binning = params.binning;

        AbyzError err = Abyz_SetAcquisitionParams(sdkHandle_, &abyzParams);
        if (err != ABYZ_OK) {
            setError(mapAbyzError(err), "Failed to set ABYZ acquisition parameters");
            return false;
        }
    }

    params_ = params;
    clearError();
    return true;
}

AcquisitionParams ABYZDetector::getAcquisitionParams() const {
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return params_;
}

void ABYZDetector::setListener(IDetectorListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

IDetectorListener* ABYZDetector::getListener() const {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

bool ABYZDetector::startAcquisition() {
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
    AbyzError err = Abyz_StartAcquisition(sdkHandle_);
    if (err != ABYZ_OK) {
        setError(mapAbyzError(err), "Failed to start ABYZ acquisition");
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

bool ABYZDetector::stopAcquisition() {
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
    AbyzError err = Abyz_StopAcquisition(sdkHandle_);
    if (err != ABYZ_OK) {
        setError(mapAbyzError(err), "Failed to stop ABYZ acquisition");
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

bool ABYZDetector::isAcquiring() const {
    if (sdkHandle_) {
        int acquiring = 0;
        if (Abyz_IsAcquiring(sdkHandle_, &acquiring) == ABYZ_OK) {
            return acquiring != 0;
        }
    }
    return state_.load() == DetectorState::ACQUIRING;
}

std::shared_ptr<IDetectorSynchronous> ABYZDetector::getSynchronousInterface() {
    return syncInterface_;
}

ErrorInfo ABYZDetector::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void ABYZDetector::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";
    lastError_.details.clear();
}

//=============================================================================
// Callback Bridges
//=============================================================================

void ABYZDetector::imageCallbackBridge(const AbyzImage* img, void* ctx) {
    if (!ctx || !img) return;
    auto* detector = static_cast<ABYZDetector*>(ctx);
    detector->onImageReceived(img);
}

void ABYZDetector::stateCallbackBridge(AbyzState sdkState, void* ctx) {
    if (!ctx) return;
    auto* detector = static_cast<ABYZDetector*>(ctx);
    detector->onStateChanged(sdkState);
}

void ABYZDetector::errorCallbackBridge(AbyzError err, const char* msg, void* ctx) {
    if (!ctx) return;
    auto* detector = static_cast<ABYZDetector*>(ctx);
    detector->onError(err, msg);
}

//=============================================================================
// Instance Callback Handlers
//=============================================================================

void ABYZDetector::onImageReceived(const AbyzImage* img) {
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

void ABYZDetector::onStateChanged(AbyzState sdkState) {
    DetectorState newState = mapAbyzState(sdkState);
    state_ = newState;
    notifyStateChanged(newState);
}

void ABYZDetector::onError(AbyzError err, const char* msg) {
    ErrorInfo error;
    error.code = mapAbyzError(err);
    error.message = msg ? msg : Abyz_ErrorToString(err);
    error.details = "ABYZ SDK error";

    {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = error;
    }

    notifyError(error);
}

//=============================================================================
// Private Helper Methods
//=============================================================================

void ABYZDetector::setError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = code;
    lastError_.message = message;
    lastError_.details.clear();
}

void ABYZDetector::notifyStateChanged(DetectorState newState) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onStateChanged(newState);
    }
}

void ABYZDetector::notifyError(const ErrorInfo& error) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onError(error);
    }
}

void ABYZDetector::notifyImageReceived(const ImageData& image) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onImageReceived(image);
    }
}

std::string ABYZDetector::stateToString(DetectorState state) const {
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

ErrorCode ABYZDetector::mapAbyzError(AbyzError abyzError) const {
    switch (abyzError) {
        case ABYZ_OK: return ErrorCode::SUCCESS;
        case ABYZ_ERR_NOT_INITIALIZED: return ErrorCode::NOT_INITIALIZED;
        case ABYZ_ERR_ALREADY_INITIALIZED: return ErrorCode::ALREADY_INITIALIZED;
        case ABYZ_ERR_INVALID_PARAMETER: return ErrorCode::INVALID_PARAMETER;
        case ABYZ_ERR_TIMEOUT: return ErrorCode::TIMEOUT;
        case ABYZ_ERR_HARDWARE: return ErrorCode::HARDWARE_ERROR;
        case ABYZ_ERR_COMMUNICATION: return ErrorCode::COMMUNICATION_ERROR;
        case ABYZ_ERR_NOT_SUPPORTED: return ErrorCode::NOT_SUPPORTED;
        case ABYZ_ERR_STATE_ERROR: return ErrorCode::STATE_ERROR;
        case ABYZ_ERR_OUT_OF_MEMORY: return ErrorCode::OUT_OF_MEMORY;
        case ABYZ_ERR_UNKNOWN_VENDOR: return ErrorCode::INVALID_PARAMETER;
        default: return ErrorCode::UNKNOWN_ERROR;
    }
}

DetectorState ABYZDetector::mapAbyzState(AbyzState abyzState) const {
    switch (abyzState) {
        case ABYZ_STATE_IDLE: return DetectorState::IDLE;
        case ABYZ_STATE_READY: return DetectorState::READY;
        case ABYZ_STATE_ACQUIRING: return DetectorState::ACQUIRING;
        case ABYZ_STATE_ERROR: return DetectorState::ERROR;
        default: return DetectorState::UNKNOWN;
    }
}

//=============================================================================
// ABYZDetectorSynchronous Implementation
//=============================================================================

ABYZDetectorSynchronous::ABYZDetectorSynchronous(ABYZDetector* detector)
    : detector_(detector)
    , cancelled_(false)
{
}

bool ABYZDetectorSynchronous::acquireFrame(ImageData& outImage, uint32_t timeoutMs) {
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

    // For callback-based SDK with listener, wait for callback delivery
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while (!cancelled_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) {
            detector_->setError(ErrorCode::TIMEOUT, "Frame acquisition timeout");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (detector_->getListener()) {
            return true;
        }
    }

    return !cancelled_;
}

bool ABYZDetectorSynchronous::acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs) {
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

bool ABYZDetectorSynchronous::cancelAcquisition() {
    cancelled_ = true;
    return true;
}
