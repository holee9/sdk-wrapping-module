#pragma once

#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include "uxdi/IDetectorSynchronous.h"
#include "uxdi/Types.h"
#include "vieworks_sdk.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

namespace uxdi::adapters::vieworks {

/**
 * @brief Vieworks detector implementation
 *
 * Wraps the Vieworks X-ray detector SDK with polling-based frame retrieval.
 * Uses background thread to poll for new frames and deliver them via listener.
 */
class VieworksDetector : public IDetector {
    // Allow VieworksDetectorSynchronous to access private helper methods
    friend class VieworksDetectorSynchronous;

public:
    VieworksDetector();
    virtual ~VieworksDetector();

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
    // SDK handle
    VieworksHandle sdkHandle_;
    std::mutex sdkMutex_;

    // State management
    std::atomic<DetectorState> state_;
    mutable std::mutex stateMutex_;

    // Initialization status
    std::atomic<bool> initialized_;
    std::atomic<bool> sdkInitialized_;

    // Listener management
    IDetectorListener* listener_;
    mutable std::mutex listenerMutex_;

    // Acquisition parameters
    AcquisitionParams params_;
    mutable std::mutex paramsMutex_;

    // Error handling
    ErrorInfo lastError_;
    mutable std::mutex errorMutex_;

    // Synchronous interface
    std::shared_ptr<IDetectorSynchronous> syncInterface_;

    // Polling thread
    std::thread pollingThread_;
    std::atomic<bool> pollingActive_;

    // Polling thread function
    void pollingThreadFunc();

    // Helper methods
    void setError(ErrorCode code, const std::string& message);
    void notifyStateChanged(DetectorState newState);
    void notifyError(const ErrorInfo& error);
    void notifyImageReceived(const ImageData& image);
    std::string stateToString(DetectorState state) const;

    // Error code mapping
    ErrorCode mapVieworksError(VieworksStatus status) const;
    DetectorState mapVieworksState(VieworksState vieworksState) const;
};

/**
 * @brief Synchronous acquisition interface for VieworksDetector
 */
class VieworksDetectorSynchronous : public IDetectorSynchronous {
public:
    explicit VieworksDetectorSynchronous(VieworksDetector* detector);
    virtual ~VieworksDetectorSynchronous() = default;

    bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) override;
    bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) override;
    bool cancelAcquisition() override;

private:
    VieworksDetector* detector_;
    std::atomic<bool> cancelled_;
};

} // namespace uxdi::adapters::vieworks
