#include "EmulDetector.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>

namespace uxdi::adapters::emul {

//=============================================================================
// EmulDetector Implementation
//=============================================================================

// Default scenario JSON (used when no config provided)
static const char* DEFAULT_SCENARIO_JSON = R"(
{
  "name": "Default Emulator Scenario",
  "description": "Simple frame generation for emulator",
  "actions": [
    {"type": "set_state", "state": "ready"},
    {"type": "acquire", "count": 100, "interval_ms": 33}
  ]
}
)";

EmulDetector::EmulDetector(const std::string& config)
    : scenarioEngine_()
    , scenarioConfig_(config)
    , state_(DetectorState::IDLE)
    , initialized_(false)
    , listener_(nullptr)
    , acquisitionActive_(false)
    , syncInterface_(std::make_shared<EmulDetectorSynchronous>(this))
{
    // Initialize default acquisition parameters
    params_.width = 1024;
    params_.height = 1024;
    params_.offsetX = 0;
    params_.offsetY = 0;
    params_.exposureTimeMs = 100.0f;
    params_.gain = 1.0f;
    params_.binning = 1;

    // Initialize detector info
    detectorInfo_.vendor = "UXDI";
    detectorInfo_.model = "EMUL-001";
    detectorInfo_.firmwareVersion = "1.0.0";
    detectorInfo_.maxWidth = 4096;
    detectorInfo_.maxHeight = 4096;
    detectorInfo_.bitDepth = 16;

    // Generate serial number based on timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    serialNumber_ = "EMUL-" + std::to_string(timestamp);
    detectorInfo_.serialNumber = serialNumber_;

    // Initialize error info
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";

    // Configure frame generation
    scenarioEngine_.SetFrameConfig(params_.width, params_.height, 16);
}

EmulDetector::~EmulDetector() {
    if (initialized_) {
        shutdown();
    }
}

bool EmulDetector::initialize() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (initialized_) {
        setError(ErrorCode::ALREADY_INITIALIZED, "Detector is already initialized");
        return false;
    }

    state_ = DetectorState::INITIALIZING;

    // Load scenario from config or use default
    if (!scenarioConfig_.empty()) {
        if (!loadScenarioFromConfig(scenarioConfig_)) {
            // Fall back to default scenario on error
            if (!loadDefaultScenario()) {
                state_ = DetectorState::ERROR;
                setError(ErrorCode::INVALID_PARAMETER, "Failed to load scenario configuration");
                return false;
            }
        }
    } else {
        if (!loadDefaultScenario()) {
            state_ = DetectorState::ERROR;
            setError(ErrorCode::UNKNOWN_ERROR, "Failed to load default scenario");
            return false;
        }
    }

    initialized_ = true;
    state_ = DetectorState::READY;
    clearError();

    notifyStateChanged(DetectorState::READY);
    return true;
}

bool EmulDetector::shutdown() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    // Stop acquisition if running
    if (state_ == DetectorState::ACQUIRING) {
        stopAcquisition();
    }

    // Wait for acquisition thread to finish
    if (acquisitionThread_.joinable()) {
        acquisitionThread_.join();
    }

    initialized_ = false;
    state_ = DetectorState::IDLE;

    notifyStateChanged(DetectorState::IDLE);
    clearError();
    return true;
}

bool EmulDetector::isInitialized() const {
    return initialized_.load();
}

DetectorInfo EmulDetector::getDetectorInfo() const {
    return detectorInfo_;
}

std::string EmulDetector::getVendorName() const {
    return "UXDI";
}

std::string EmulDetector::getModelName() const {
    return "EMUL-001";
}

DetectorState EmulDetector::getState() const {
    // Return scenario engine state if running, otherwise return internal state
    if (acquisitionActive_.load()) {
        return scenarioEngine_.GetCurrentState();
    }
    return state_.load();
}

std::string EmulDetector::getStateString() const {
    return stateToString(getState());
}

bool EmulDetector::setAcquisitionParams(const AcquisitionParams& params) {
    std::lock_guard<std::mutex> lock(paramsMutex_);

    // Validate parameters
    if (params.width == 0 || params.height == 0) {
        setError(ErrorCode::INVALID_PARAMETER, "Width and height must be non-zero");
        return false;
    }

    if (params.width > detectorInfo_.maxWidth || params.height > detectorInfo_.maxHeight) {
        setError(ErrorCode::INVALID_PARAMETER, "Resolution exceeds maximum supported");
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

    // Update scenario engine frame configuration
    scenarioEngine_.SetFrameConfig(params_.width, params_.height, detectorInfo_.bitDepth);

    clearError();
    return true;
}

AcquisitionParams EmulDetector::getAcquisitionParams() const {
    std::lock_guard<std::mutex> lock(paramsMutex_);
    return params_;
}

void EmulDetector::setListener(IDetectorListener* listener) {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

IDetectorListener* EmulDetector::getListener() const {
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

bool EmulDetector::startAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (state_ == DetectorState::ACQUIRING || acquisitionActive_.load()) {
        setError(ErrorCode::STATE_ERROR, "Acquisition is already in progress");
        return false;
    }

    if (state_ != DetectorState::READY) {
        setError(ErrorCode::STATE_ERROR, "Detector must be in READY state to start acquisition");
        return false;
    }

    // Start scenario engine
    scenarioEngine_.Start();
    acquisitionActive_ = true;
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

    // Start frame generation thread
    if (acquisitionThread_.joinable()) {
        acquisitionThread_.join();
    }
    acquisitionThread_ = std::thread(&EmulDetector::acquisitionThreadFunc, this);

    return true;
}

bool EmulDetector::stopAcquisition() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (!initialized_) {
        setError(ErrorCode::NOT_INITIALIZED, "Detector is not initialized");
        return false;
    }

    if (!acquisitionActive_.load() && state_ != DetectorState::ACQUIRING) {
        setError(ErrorCode::STATE_ERROR, "No acquisition is in progress");
        return false;
    }

    // Stop scenario engine
    scenarioEngine_.Stop();
    acquisitionActive_ = false;
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

bool EmulDetector::isAcquiring() const {
    return acquisitionActive_.load() || state_.load() == DetectorState::ACQUIRING;
}

std::shared_ptr<IDetectorSynchronous> EmulDetector::getSynchronousInterface() {
    return syncInterface_;
}

ErrorInfo EmulDetector::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void EmulDetector::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = ErrorCode::SUCCESS;
    lastError_.message = "No error";
    lastError_.details.clear();
}

//=============================================================================
// Private Helper Methods
//=============================================================================

void EmulDetector::setError(ErrorCode code, const std::string& message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.code = code;
    lastError_.message = message;
    lastError_.details.clear();
}

void EmulDetector::notifyStateChanged(DetectorState newState) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onStateChanged(newState);
    }
}

void EmulDetector::notifyError(const ErrorInfo& error) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onError(error);
    }
}

void EmulDetector::notifyImageReceived(const ImageData& image) {
    IDetectorListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listener = listener_;
    }

    if (listener) {
        listener->onImageReceived(image);
    }
}

std::string EmulDetector::stateToString(DetectorState state) const {
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

bool EmulDetector::loadScenarioFromConfig(const std::string& config) {
    // Trim whitespace
    std::string trimmedConfig = config;
    trimmedConfig.erase(0, trimmedConfig.find_first_not_of(" \t\n\r"));
    trimmedConfig.erase(trimmedConfig.find_last_not_of(" \t\n\r") + 1);

    if (trimmedConfig.empty()) {
        return loadDefaultScenario();
    }

    // Check if it's a file reference (file:// prefix or looks like a file path)
    if (trimmedConfig.find("file://") == 0) {
        std::string filePath = trimmedConfig.substr(7);  // Remove "file://" prefix
        return scenarioEngine_.LoadScenarioFromFile(filePath);
    }

    // Check if it contains "scenario_file" key
    size_t fileKeyPos = trimmedConfig.find("\"scenario_file\"");
    if (fileKeyPos != std::string::npos) {
        // Extract file path from JSON
        size_t colonPos = trimmedConfig.find(':', fileKeyPos);
        if (colonPos != std::string::npos) {
            size_t quoteStart = trimmedConfig.find('\"', colonPos);
            if (quoteStart != std::string::npos) {
                size_t quoteEnd = trimmedConfig.find('\"', quoteStart + 1);
                if (quoteEnd != std::string::npos) {
                    std::string filePath = trimmedConfig.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    return scenarioEngine_.LoadScenarioFromFile(filePath);
                }
            }
        }
    }

    // Check if it contains "scenario" key (inline scenario)
    size_t scenarioKeyPos = trimmedConfig.find("\"scenario\"");
    if (scenarioKeyPos != std::string::npos) {
        size_t scenarioStart = trimmedConfig.find('{', scenarioKeyPos);
        if (scenarioStart != std::string::npos) {
            // Find matching closing brace
            int depth = 0;
            size_t scenarioEnd = scenarioStart;
            for (; scenarioEnd < trimmedConfig.length(); ++scenarioEnd) {
                if (trimmedConfig[scenarioEnd] == '{') depth++;
                else if (trimmedConfig[scenarioEnd] == '}') depth--;
                if (depth == 0) break;
            }
            if (depth == 0) {
                std::string scenarioJson = trimmedConfig.substr(scenarioStart, scenarioEnd - scenarioStart + 1);
                return scenarioEngine_.LoadScenario(scenarioJson);
            }
        }
    }

    // Treat entire config as scenario JSON
    return scenarioEngine_.LoadScenario(trimmedConfig);
}

bool EmulDetector::loadDefaultScenario() {
    return scenarioEngine_.LoadScenario(DEFAULT_SCENARIO_JSON);
}

std::string EmulDetector::getFileContents(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void EmulDetector::acquisitionThreadFunc() {
    while (acquisitionActive_.load()) {
        // Check for error injection
        auto error = scenarioEngine_.GetNextError();
        if (error) {
            ErrorInfo errorInfo;
            errorInfo.code = *error;
            errorInfo.message = "Scenario error injection";
            errorInfo.details = "Error injected by scenario engine";

            setError(*error, "Scenario error injection");
            notifyError(errorInfo);

            // Stop acquisition on error
            acquisitionActive_ = false;
            state_ = DetectorState::ERROR;
            notifyStateChanged(DetectorState::ERROR);
            break;
        }

        // Get next frame from scenario engine
        auto frameData = scenarioEngine_.GetNextFrame();
        if (frameData) {
            ImageData image = convertFrameDataToImageData(*frameData);
            notifyImageReceived(image);
        } else {
            // No frame generated this iteration
            if (scenarioEngine_.IsComplete()) {
                // Scenario completed
                break;
            }
            // Sleep a bit to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Check if we're still supposed to be acquiring
    if (acquisitionActive_.load() && state_.load() == DetectorState::ACQUIRING) {
        // Transition to ready state
        state_ = DetectorState::READY;
        notifyStateChanged(DetectorState::READY);
    }
}

ImageData EmulDetector::convertFrameDataToImageData(const FrameData& frameData) {
    ImageData image;
    image.width = frameData.width;
    image.height = frameData.height;
    image.bitDepth = frameData.bitDepth;
    image.frameNumber = frameData.frameNumber;
    image.timestamp = frameData.timestamp;
    image.data = frameData.data;  // shared_ptr copy
    image.dataLength = frameData.dataLength;
    return image;
}

//=============================================================================
// EmulDetectorSynchronous Implementation
//=============================================================================

EmulDetectorSynchronous::EmulDetectorSynchronous(EmulDetector* detector)
    : detector_(detector)
    , cancelled_(false)
{
}

bool EmulDetectorSynchronous::acquireFrame(ImageData& outImage, uint32_t timeoutMs) {
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

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    // Wait for frame from scenario engine
    while (!cancelled_) {
        // Access scenario engine through friend declaration
        auto frameData = detector_->scenarioEngine_.GetNextFrame();
        if (frameData) {
            outImage = detector_->convertFrameDataToImageData(*frameData);

            // Notify listener if set
            auto listener = detector_->getListener();
            if (listener) {
                listener->onImageReceived(outImage);
            }

            return true;
        }

        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) {
            detector_->setError(ErrorCode::TIMEOUT, "Frame acquisition timeout");
            return false;
        }

        // Sleep a bit to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

bool EmulDetectorSynchronous::acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs) {
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

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);

    // Acquire frames
    for (uint32_t i = 0; i < frameCount; ++i) {
        if (cancelled_) {
            break;
        }

        // Check remaining timeout
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) {
            detector_->setError(ErrorCode::TIMEOUT, "Multi-frame acquisition timeout");
            return false;
        }

        uint32_t remainingTimeout = timeoutMs - static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

        ImageData frame;
        if (!acquireFrame(frame, remainingTimeout)) {
            return false;
        }

        outImages.push_back(std::move(frame));
    }

    return !cancelled_ && outImages.size() == frameCount;
}

bool EmulDetectorSynchronous::cancelAcquisition() {
    cancelled_ = true;
    return true;
}

} // namespace uxdi::adapters::emul
