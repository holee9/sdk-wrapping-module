#pragma once

#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include "uxdi/IDetectorSynchronous.h"
#include "uxdi/Types.h"
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>

namespace uxdi::adapters::dummy {

/**
 * @brief Dummy detector implementation for testing without hardware
 *
 * DummyDetector provides a complete IDetector implementation that
 * generates static black frames without requiring actual hardware.
 * Useful for unit testing and integration testing.
 */
class DummyDetector : public IDetector {
    // Allow DummyDetectorSynchronous to access private helper methods
    friend class DummyDetectorSynchronous;

public:
    DummyDetector();
    virtual ~DummyDetector();

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
    // State management
    std::atomic<DetectorState> state_;
    mutable std::mutex stateMutex_;

    // Initialization status
    std::atomic<bool> initialized_;

    // Listener management
    IDetectorListener* listener_;
    mutable std::mutex listenerMutex_;

    // Acquisition parameters
    AcquisitionParams params_;
    mutable std::mutex paramsMutex_;

    // Frame counter for generated images
    std::atomic<uint64_t> frameCounter_;

    // Error handling
    ErrorInfo lastError_;
    mutable std::mutex errorMutex_;

    // Synchronous interface
    std::shared_ptr<IDetectorSynchronous> syncInterface_;

    // Helper methods
    void setError(ErrorCode code, const std::string& message);
    void notifyStateChanged(DetectorState newState);
    void notifyError(const ErrorInfo& error);
    std::string stateToString(DetectorState state) const;

    // Frame generation
    ImageData generateBlackFrame();
};

/**
 * @brief Synchronous acquisition interface for DummyDetector
 */
class DummyDetectorSynchronous : public IDetectorSynchronous {
public:
    explicit DummyDetectorSynchronous(DummyDetector* detector);
    virtual ~DummyDetectorSynchronous() = default;

    bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) override;
    bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) override;
    bool cancelAcquisition() override;

private:
    DummyDetector* detector_;
    std::atomic<bool> cancelled_;
};

} // namespace uxdi::adapters::dummy
