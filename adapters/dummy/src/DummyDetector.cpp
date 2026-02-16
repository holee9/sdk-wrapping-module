#include "DummyDetector.h"
#include <cstring>
#include <chrono>
#include <thread>

namespace uxdi::adapters::dummy {

//=============================================================================
// DummyDetector Implementation
//=============================================================================

DummyDetector::DummyDetector()
    : state_(DetectorState::IDLE)
    , initialized_(false)
    , listener_(nullptr)
    , frameCounter_(0)
    , syncInterface_(std::make_shared<DummyDetectorSynchronous>(this))
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
}

DummyDetector::~DummyDetector() {
    if (initialized_) {
        shutdown();
    }
}

bool DummyDetector::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (initialized_) {
        setError(ErrorCode::ALREADY_INITIALIZED, "Detector is already initialized");
        return false;
    }

    // Simulate initialization
    state_ = DetectorState::INITIALIZING;

    // In a real detector, this would initialize hardware
    // For dummy, we just mark as initialized and ready
    initialized_ = true;
    state_ = DetectorState::READY;
    clearError();

    notifyStateChanged(DetectorState::READY);
    return true;
}

bool DummyDetector::shutdown() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    // Stop acquisition if running
    if (state_ == DetectorState::ACQUIRING) {
        stopAcquisition();
    }

    initialized_ = false;
    state_ = DetectorState::IDLE;
    frameCounter_ = 0;

    notifyStateChanged(DetectorState::IDLE);
    clearError();
    return true;
}

bool DummyDetector::isInitialized() const {
    return initialized_.load();
}

DetectorInfo DummyDetector::getDetectorInfo() const {
    DetectorInfo info;
    info.vendor = "UXDI";
    info.model = "DUMMY-001";
    info.serialNumber = "DUMMY-001-TEST";
    info.firmwareVersion = "1.0.0";
    info.maxWidth = 1024;
    info.maxHeight = 1024;
    info.bitDepth = 16;
    return info;
}

std::string DummyDetector::getVendorName() const {
    return "UXDI";
}

std::string DummyDetector::getModelName() const {
    return "DUMMY-001";
}

DetectorState DummyDetector::getState() const {
    return state_.load();
}

std::string DummyDetector::getStateString() const {
    return stateToString(state_.load());
}

bool DummyDetector::setAcquisitionParams(const AcquisitionParams& params) {
    std::lock_guard<std::mutex> lock(paramsMutex_);

    // Validate parameters
    if (params.width == 0 || params.height == 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Width and height must be non-zero");
        return false;
    }

    if (params.width > 1024 || params.height > 1024) {
        setError(ErrorCode::INVALID_PARAMETER, "Maximum resolution is 1024x1024");
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

    // Valid binning factors: 1, 2, 4
    if (params.binning != 1 && params.binning != 2 && params.binning != 4) {
        setError(ErrorCode::INVALID_PARAMETER, "Binning must be 1, 2, or 4");
        return false;
    }

    params_ = params;
    clearError();
    return true;
}

AcquisitionParams DummyDetector::getAcquisitionParams() const {
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return params_;
}

void DummyDetector::setListener(IDetectorListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

IDetectorListener* DummyDetector::getListener() const {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

bool DummyDetector::startAcquisition() {
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

bool DummyDetector::stopAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (state_ != DetectorState::ACQUIRING) {
        setError(ErrorCode::STATE_ERROR, "No acquisition is in progress");
        return false;
    }

    state_ = DetectorState::STOPPING;

    // Notify listener that acquisition stopped
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> listenerLock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onAcquisitionStopped();
    }

    state_ = DetectorState::READY;
    notifyStateChanged(DetectorState::READY);
    clearError();
    return true;
}

bool DummyDetector::isAcquiring() const {
    return state_.load() == DetectorState::ACQUIRING;
}

std::shared_ptr<IDetectorSynchronous> DummyDetector::getSynchronousInterface() {
    return syncInterface_;
}

ErrorInfo DummyDetector::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void DummyDetector::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";
    lastError_.details.clear();
}

//=============================================================================
// Private Helper Methods
//=============================================================================

void DummyDetector::setError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = code;
    lastError_.message = message;
    lastError_.details.clear();
}

void DummyDetector::notifyStateChanged(DetectorState newState) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onStateChanged(newState);
    }
}

void DummyDetector::notifyError(const ErrorInfo& error) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onError(error);
    }
}

std::string DummyDetector::stateToString(DetectorState state) const {
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

ImageData DummyDetector::generateBlackFrame() {
    // Get current acquisition parameters
    AcquisitionParams params;
    {
        std::lock_guard<std::mutex> lock(paramsMutex_);
        params = params_;
    }

    // Calculate frame size (16-bit grayscale)
    const size_t bytesPerPixel = 2;
    const size_t frameSize = params.width * params.height * bytesPerPixel;

    // Allocate black frame buffer
    auto buffer = std::shared_ptr<uint8_t[]>(new uint8_t[frameSize]);
    std::memset(buffer.get(), 0, frameSize);

    // Create image data structure
    ImageData image;
    image.width = params.width;
    image.height = params.height;
    image.bitDepth = 16;
    image.frameNumber = frameCounter_++;
    image.timestamp = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    image.data = buffer;
    image.dataLength = frameSize;

    return image;
}

//=============================================================================
// DummyDetectorSynchronous Implementation
//=============================================================================

DummyDetectorSynchronous::DummyDetectorSynchronous(DummyDetector* detector)
    : detector_(detector)
    , cancelled_(false)
{
}

bool DummyDetectorSynchronous::acquireFrame(ImageData& outImage, uint32_t timeoutMs) {
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

    // Simulate exposure delay
    auto params = detector_->getAcquisitionParams();
    auto sleepTime = std::chrono::duration<double, std::milli>(params.exposureTimeMs);
    std::this_thread::sleep_for(sleepTime);

    if (cancelled_) {
        return false;
    }

    // Generate and return black frame
    outImage = detector_->generateBlackFrame();

    // Notify listener if set
    auto listener = detector_->getListener();
    if (listener) {
        listener->onImageReceived(outImage);
    }

    return true;
}

bool DummyDetectorSynchronous::acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs) {
    if (!detector_) {
        return false;
    }

    cancelled_ = false;
    outImages.clear();
    outImages.reserve(frameCount);

    // Ensure detector is in acquiring state
    if (detector_->getState() != DetectorState::ACQUIRING) {
        if (!detector_->startAcquisition()) {
            return false;
        }
    }

    // Acquire frames
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

bool DummyDetectorSynchronous::cancelAcquisition() {
    cancelled_ = true;
    return true;
}

} // namespace uxdi::adapters::dummy
