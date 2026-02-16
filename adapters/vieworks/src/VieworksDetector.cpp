#include "VieworksDetector.h"
#include "vieworks_sdk.h"
#include <cstring>
#include <chrono>

using namespace uxdi;
using namespace uxdi::adapters::vieworks;

//=============================================================================
// VieworksDetector Implementation
//=============================================================================

VieworksDetector::VieworksDetector()
    : sdkHandle_(nullptr)
    , state_(DetectorState::IDLE)
    , initialized_(false)
    , sdkInitialized_(false)
    , listener_(nullptr)
    , pollingActive_(false)
    , syncInterface_(std::make_shared<VieworksDetectorSynchronous>(this))
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

    // Initialize Vieworks SDK
    if (Vieworks_Initialize() == VIEWORKS_OK) {
        sdkInitialized_ = true;
    }
}

VieworksDetector::~VieworksDetector() {
    // Stop polling thread
    if (pollingActive_.load()) {
        pollingActive_ = false;
        if (pollingThread_.joinable()) {
            pollingThread_.join();
        }
    }

    if (initialized_) {
        shutdown();
    }

    // Cleanup SDK handle
    if (sdkHandle_) {
        Vieworks_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
    }

    // Shutdown SDK
    if (sdkInitialized_) {
        Vieworks_Shutdown();
    }
}

bool VieworksDetector::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (initialized_) {
        setError(ErrorCode::ALREADY_INITIALIZED, "Detector is already initialized");
        return false;
    }

    if (!sdkInitialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Vieworks SDK initialization failed");
        return false;
    }

    state_ = DetectorState::INITIALIZING;

    // Create SDK detector handle
    VieworksStatus status = Vieworks_CreateDetector("", &sdkHandle_);
    if (status != VIEWORKS_OK || !sdkHandle_) {
        setError(mapVieworksError(status), "Failed to create Vieworks detector");
        state_ = DetectorState::ERROR;
        return false;
    }

    // Initialize the detector
    status = Vieworks_InitializeDetector(sdkHandle_);
    if (status != VIEWORKS_OK) {
        setError(mapVieworksError(status), "Failed to initialize Vieworks detector");
        Vieworks_DestroyDetector(sdkHandle_);
        sdkHandle_ = nullptr;
        state_ = DetectorState::ERROR;
        return false;
    }

    // Set initial acquisition parameters
    VieworksAcqParams vieworksParams;
    vieworksParams.width = params_.width;
    vieworksParams.height = params_.height;
    vieworksParams.offsetX = params_.offsetX;
    vieworksParams.offsetY = params_.offsetY;
    vieworksParams.exposureTimeMs = params_.exposureTimeMs;
    vieworksParams.gain = params_.gain;
    vieworksParams.binning = params_.binning;

    status = Vieworks_SetAcquisitionParams(sdkHandle_, &vieworksParams);
    if (status != VIEWORKS_OK) {
        setError(mapVieworksError(status), "Failed to set Vieworks acquisition parameters");
        Vieworks_ShutdownDetector(sdkHandle_);
        Vieworks_DestroyDetector(sdkHandle_);
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

bool VieworksDetector::shutdown() {
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
        Vieworks_ShutdownDetector(sdkHandle_);
    }

    initialized_ = false;
    state_ = DetectorState::IDLE;

    notifyStateChanged(DetectorState::IDLE);
    clearError();
    return true;
}

bool VieworksDetector::isInitialized() const {
    return initialized_.load();
}

DetectorInfo VieworksDetector::getDetectorInfo() const {
    DetectorInfo info{};
    info.vendor = "Vieworks";
    info.model = "Mock-VIVIX";
    info.serialNumber = "VIEWORKS-MOCK-001";
    info.firmwareVersion = "1.5.2";
    info.maxWidth = 4096;
    info.maxHeight = 4096;
    info.bitDepth = 16;

    // Try to get actual info from SDK
    if (sdkHandle_) {
        VieworksDetectorInfo vieworksInfo;
        if (Vieworks_GetDetectorInfo(sdkHandle_, &vieworksInfo) == VIEWORKS_OK) {
            info.vendor = vieworksInfo.vendor;
            info.model = vieworksInfo.model;
            info.serialNumber = vieworksInfo.serialNumber;
            info.firmwareVersion = vieworksInfo.firmwareVersion;
            info.maxWidth = vieworksInfo.maxWidth;
            info.maxHeight = vieworksInfo.maxHeight;
            info.bitDepth = vieworksInfo.bitDepth;
        }
    }

    return info;
}

std::string VieworksDetector::getVendorName() const {
    return "Vieworks";
}

std::string VieworksDetector::getModelName() const {
    return "Mock-VIVIX";
}

DetectorState VieworksDetector::getState() const {
    return state_.load();
}

std::string VieworksDetector::getStateString() const {
    return stateToString(state_.load());
}

bool VieworksDetector::setAcquisitionParams(const AcquisitionParams& params) {
    std::lock_guard<std::mutex> lock(paramsMutex_);

    // Validate parameters
    if (params.width == 0 || params.height == 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Width and height must be non-zero");
        return false;
    }

    if (params.width > 4096 || params.height > 4096) {
        setError(ErrorCode::INVALID_PARAMETER, "Maximum resolution is 4096x4096");
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
        VieworksAcqParams vieworksParams;
        vieworksParams.width = params.width;
        vieworksParams.height = params.height;
        vieworksParams.offsetX = params.offsetX;
        vieworksParams.offsetY = params.offsetY;
        vieworksParams.exposureTimeMs = params.exposureTimeMs;
        vieworksParams.gain = params.gain;
        vieworksParams.binning = params.binning;

        VieworksStatus status = Vieworks_SetAcquisitionParams(sdkHandle_, &vieworksParams);
        if (status != VIEWORKS_OK) {
            setError(mapVieworksError(status), "Failed to set Vieworks acquisition parameters");
            return false;
        }
    }

    params_ = params;
    clearError();
    return true;
}

AcquisitionParams VieworksDetector::getAcquisitionParams() const {
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return params_;
}

void VieworksDetector::setListener(IDetectorListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

IDetectorListener* VieworksDetector::getListener() const {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

bool VieworksDetector::startAcquisition() {
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
    VieworksStatus status = Vieworks_StartAcquisition(sdkHandle_);
    if (status != VIEWORKS_OK) {
        setError(mapVieworksError(status), "Failed to start Vieworks acquisition");
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

    // Start polling thread
    if (!pollingActive_.load()) {
        pollingActive_ = true;
        pollingThread_ = std::thread(&VieworksDetector::pollingThreadFunc, this);
    }

    return true;
}

bool VieworksDetector::stopAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (state_ != DetectorState::ACQUIRING) {
        setError(ErrorCode::STATE_ERROR, "No acquisition is in progress");
        return false;
    }

    // Stop polling thread
    pollingActive_ = false;
    if (pollingThread_.joinable()) {
        pollingThread_.join();
    }

    // Stop acquisition in SDK
    VieworksStatus status = Vieworks_StopAcquisition(sdkHandle_);
    if (status != VIEWORKS_OK) {
        setError(mapVieworksError(status), "Failed to stop Vieworks acquisition");
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

bool VieworksDetector::isAcquiring() const {
    if (sdkHandle_) {
        int acquiring = 0;
        if (Vieworks_IsAcquiring(sdkHandle_, &acquiring) == VIEWORKS_OK) {
            return acquiring != 0;
        }
    }
    return state_.load() == DetectorState::ACQUIRING;
}

std::shared_ptr<IDetectorSynchronous> VieworksDetector::getSynchronousInterface() {
    return syncInterface_;
}

ErrorInfo VieworksDetector::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void VieworksDetector::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";
    lastError_.details.clear();
}

//=============================================================================
// Polling Thread
//=============================================================================

void VieworksDetector::pollingThreadFunc() {
    while (pollingActive_.load()) {
        int ready = 0;
        VieworksStatus status = Vieworks_GetFrameReady(sdkHandle_, &ready);

        if (status == VIEWORKS_OK && ready) {
            VieworksFrame frame;
            status = Vieworks_ReadFrame(sdkHandle_, &frame);

            if (status == VIEWORKS_OK) {
                // ZERO-COPY: SDK buffer is stable until next ReadFrame
                // We can use the buffer directly without copying
                ImageData image;
                image.width = frame.width;
                image.height = frame.height;
                image.bitDepth = frame.bitDepth;
                image.frameNumber = frame.frameNumber;
                image.timestamp = frame.timestamp;
                image.dataLength = frame.dataLength;

                // Create shared_ptr<uint8_t[]> with NOOP deleter (SDK owns the memory)
                // Warning: This assumes the buffer is used before next ReadFrame
                image.data = std::shared_ptr<uint8_t[]>(
                    static_cast<uint8_t*>(frame.data),
                    [](void*) { /* SDK owns memory, no deletion */ }
                );

                notifyImageReceived(image);
            }
        }

        // Poll at ~100Hz (10ms interval)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

//=============================================================================
// Private Helper Methods
//=============================================================================

void VieworksDetector::setError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = code;
    lastError_.message = message;
    lastError_.details.clear();
}

void VieworksDetector::notifyStateChanged(DetectorState newState) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onStateChanged(newState);
    }
}

void VieworksDetector::notifyError(const ErrorInfo& error) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onError(error);
    }
}

void VieworksDetector::notifyImageReceived(const ImageData& image) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onImageReceived(image);
    }
}

std::string VieworksDetector::stateToString(DetectorState state) const {
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

ErrorCode VieworksDetector::mapVieworksError(VieworksStatus status) const {
    switch (status) {
        case VIEWORKS_OK: return ErrorCode::SUCCESS;
        case VIEWORKS_ERR_NOT_INITIALIZED: return ErrorCode::NOT_INITIALIZED;
        case VIEWORKS_ERR_ALREADY_INITIALIZED: return ErrorCode::ALREADY_INITIALIZED;
        case VIEWORKS_ERR_INVALID_PARAMETER: return ErrorCode::INVALID_PARAMETER;
        case VIEWORKS_ERR_TIMEOUT: return ErrorCode::TIMEOUT;
        case VIEWORKS_ERR_HARDWARE: return ErrorCode::HARDWARE_ERROR;
        case VIEWORKS_ERR_COMMUNICATION: return ErrorCode::COMMUNICATION_ERROR;
        case VIEWORKS_ERR_NOT_SUPPORTED: return ErrorCode::NOT_SUPPORTED;
        case VIEWORKS_ERR_STATE_ERROR: return ErrorCode::STATE_ERROR;
        case VIEWORKS_ERR_OUT_OF_MEMORY: return ErrorCode::OUT_OF_MEMORY;
        default: return ErrorCode::UNKNOWN_ERROR;
    }
}

DetectorState VieworksDetector::mapVieworksState(VieworksState vieworksState) const {
    switch (vieworksState) {
        case VIEWORKS_STATE_STANDBY: return DetectorState::IDLE;
        case VIEWORKS_STATE_READY: return DetectorState::READY;
        case VIEWORKS_STATE_EXPOSING:
        case VIEWORKS_STATE_READING: return DetectorState::ACQUIRING;
        case VIEWORKS_STATE_ERROR: return DetectorState::ERROR;
        default: return DetectorState::UNKNOWN;
    }
}

//=============================================================================
// VieworksDetectorSynchronous Implementation
//=============================================================================

VieworksDetectorSynchronous::VieworksDetectorSynchronous(VieworksDetector* detector)
    : detector_(detector)
    , cancelled_(false)
{
}

bool VieworksDetectorSynchronous::acquireFrame(ImageData& outImage, uint32_t timeoutMs) {
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

    // For polling-based SDK with listener, this is simplified
    // In production, use condition variable to wait for frame
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while (!cancelled_) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) {
            detector_->setError(ErrorCode::TIMEOUT, "Frame acquisition timeout");
            return false;
        }

        // Check if frame is ready
        if (detector_->sdkHandle_) {
            int ready = 0;
            if (Vieworks_GetFrameReady(detector_->sdkHandle_, &ready) == VIEWORKS_OK && ready) {
                VieworksFrame frame;
                if (Vieworks_ReadFrame(detector_->sdkHandle_, &frame) == VIEWORKS_OK) {
                    outImage.width = frame.width;
                    outImage.height = frame.height;
                    outImage.bitDepth = frame.bitDepth;
                    outImage.frameNumber = frame.frameNumber;
                    outImage.timestamp = frame.timestamp;
                    outImage.dataLength = frame.dataLength;
                    outImage.data = std::shared_ptr<uint8_t[]>(
                        static_cast<uint8_t*>(frame.data),
                        [](void*) { /* SDK owns memory */ }
                    );
                    return true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return !cancelled_;
}

bool VieworksDetectorSynchronous::acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs) {
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

bool VieworksDetectorSynchronous::cancelAcquisition() {
    cancelled_ = true;
    return true;
}
