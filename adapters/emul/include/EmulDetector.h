#pragma once

#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include "uxdi/IDetectorSynchronous.h"
#include "uxdi/Types.h"
#include "ScenarioEngine.h"
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <string>
#include <thread>

namespace uxdi::adapters::emul {

/**
 * @brief Emulator detector implementation using ScenarioEngine
 *
 * EmulDetector provides a complete IDetector implementation that
 * uses ScenarioEngine for realistic detector simulation based on
 * configurable test scenarios. Supports inline JSON scenarios,
 * file-based scenarios, and default behavior.
 */
class EmulDetector : public IDetector {
public:
    /**
     * @brief Construct EmulDetector with optional configuration
     * @param config JSON configuration string (inline scenario, file reference, or empty for default)
     *
     * Config formats:
     * - Inline: {"scenario": {"name": "Test", "actions": [{"type": "acquire", "count": 10}]}}
     * - File:   {"scenario_file": "scenarios/test_scenario.json"}
     * - Empty:  "" (uses built-in default scenario)
     */
    explicit EmulDetector(const std::string& config = "");
    virtual ~EmulDetector();

    // IDetector interface implementation
    bool initialize() override;
    bool shutdown() override;
    bool isInitialized() const override;

    DetectorInfo getDetectorInfo() const override;
    std::string getVendorName() const override;
    std::string getModelName() const override;

    DetectorState getState() const override;
    std::string getStateString() const override;

    bool setAcquisitionParams(const AcquisitionParams& params) override;
    AcquisitionParams getAcquisitionParams() const override;

    void setListener(IDetectorListener* listener) override;
    IDetectorListener* getListener() const override;

    bool startAcquisition() override;
    bool stopAcquisition() override;
    bool isAcquiring() const override;

    std::shared_ptr<IDetectorSynchronous> getSynchronousInterface() override;

    ErrorInfo getLastError() const override;
    void clearError() override;

private:
    // ScenarioEngine integration
    ScenarioEngine scenarioEngine_;
    std::string scenarioConfig_;

    // State management
    mutable std::mutex stateMutex_;
    std::atomic<DetectorState> state_;

    // Initialization status
    std::atomic<bool> initialized_;

    // Listener management
    IDetectorListener* listener_;
    mutable std::mutex listenerMutex_;

    // Acquisition parameters
    AcquisitionParams params_;
    mutable std::mutex paramsMutex_;

    // Detector information
    DetectorInfo detectorInfo_;
    std::string serialNumber_;

    // Error handling
    ErrorInfo lastError_;
    mutable std::mutex errorMutex_;

    // Synchronous interface
    std::shared_ptr<IDetectorSynchronous> syncInterface_;

    // Thread for frame generation
    std::atomic<bool> acquisitionActive_;
    std::thread acquisitionThread_;

    // Helper methods
    void setError(ErrorCode code, const std::string& message);
    void notifyStateChanged(DetectorState newState);
    void notifyError(const ErrorInfo& error);
    void notifyImageReceived(const ImageData& image);
    std::string stateToString(DetectorState state) const;

    // Scenario loading
    bool loadScenarioFromConfig(const std::string& config);
    bool loadDefaultScenario();
    std::string getFileContents(const std::string& filePath);

    // Frame generation thread
    void acquisitionThreadFunc();
    ImageData convertFrameDataToImageData(const FrameData& frameData);

    // Allow synchronous interface to access internals
    friend class EmulDetectorSynchronous;
};

/**
 * @brief Synchronous acquisition interface for EmulDetector
 */
class EmulDetectorSynchronous : public IDetectorSynchronous {
public:
    explicit EmulDetectorSynchronous(EmulDetector* detector);
    virtual ~EmulDetectorSynchronous() = default;

    bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) override;
    bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) override;
    bool cancelAcquisition() override;

private:
    EmulDetector* detector_;
    std::atomic<bool> cancelled_;
};

} // namespace uxdi::adapters::emul
