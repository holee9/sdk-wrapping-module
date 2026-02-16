#pragma once

#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include "uxdi/IDetectorSynchronous.h"
#include "uxdi/Types.h"
#include "abyz_sdk.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

namespace uxdi::adapters::abyz {

/**
 * @brief ABYZ detector implementation (multi-vendor: Rayence, Samsung, DRTech)
 *
 * Wraps the ABYZ X-ray detector SDK with callback-based image delivery.
 * Supports multiple vendors selected via configuration.
 */
class ABYZDetector : public IDetector {
    // Allow ABYZDetectorSynchronous to access private helper methods
    friend class ABYZDetectorSynchronous;

public:
    /**
     * @brief Construct ABYZDetector with vendor configuration
     * @param config JSON configuration string with "vendor" field
     *               Examples: {"vendor": "rayence"}, {"vendor": "samsung"}
     */
    explicit ABYZDetector(const std::string& config = "");
    virtual ~ABYZDetector();

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
    // Configuration
    std::string config_;

    // SDK handle
    AbyzHandle sdkHandle_;
    std::mutex sdkMutex_;

    // State management
    std::atomic<DetectorState> state_;
    mutable std::mutex stateMutex_;

    // Initialization status
    std::atomic<bool> initialized_;
    std::atomic<bool> sdkInitialized_;

    // Vendor information
    AbyzDetectorInfo vendorInfo_;

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

    // SDK callback bridges (static for C compatibility)
    static void imageCallbackBridge(const AbyzImage* img, void* ctx);
    static void stateCallbackBridge(AbyzState sdkState, void* ctx);
    static void errorCallbackBridge(AbyzError err, const char* msg, void* ctx);

    // Instance callback handlers
    void onImageReceived(const AbyzImage* img);
    void onStateChanged(AbyzState sdkState);
    void onError(AbyzError err, const char* msg);

    // Helper methods
    void setError(ErrorCode code, const std::string& message);
    void notifyStateChanged(DetectorState newState);
    void notifyError(const ErrorInfo& error);
    void notifyImageReceived(const ImageData& image);
    std::string stateToString(DetectorState state) const;

    // Error code mapping
    ErrorCode mapAbyzError(AbyzError abyzError) const;
    DetectorState mapAbyzState(AbyzState abyzState) const;
};

/**
 * @brief Synchronous acquisition interface for ABYZDetector
 */
class ABYZDetectorSynchronous : public IDetectorSynchronous {
public:
    explicit ABYZDetectorSynchronous(ABYZDetector* detector);
    virtual ~ABYZDetectorSynchronous() = default;

    bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) override;
    bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) override;
    bool cancelAcquisition() override;

private:
    ABYZDetector* detector_;
    std::atomic<bool> cancelled_;
};

} // namespace uxdi::adapters::abyz
