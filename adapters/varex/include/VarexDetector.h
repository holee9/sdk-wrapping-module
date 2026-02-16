#pragma once

#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include "uxdi/IDetectorSynchronous.h"
#include "uxdi/Types.h"
#include "varex_sdk.h"
#include <memory>
#include <mutex>
#include <atomic>

namespace uxdi::adapters::varex {

/**
 * @brief Varex detector implementation
 *
 * Wraps the Varex X-ray detector SDK with callback-based image delivery.
 * The SDK owns image buffers, so this adapter implements mandatory copy strategy.
 */
class VarexDetector : public IDetector {
    // Allow VarexDetectorSynchronous to access private helper methods
    friend class VarexDetectorSynchronous;

public:
    VarexDetector();
    virtual ~VarexDetector();

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
    VarexHandle sdkHandle_;
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

    // SDK callback bridges (static for C compatibility)
    static void imageCallbackBridge(const VarexImage* img, void* ctx);
    static void stateCallbackBridge(VarexState sdkState, void* ctx);
    static void errorCallbackBridge(VarexError err, const char* msg, void* ctx);

    // Instance callback handlers
    void onImageReceived(const VarexImage* img);
    void onStateChanged(VarexState sdkState);
    void onError(VarexError err, const char* msg);

    // Helper methods
    void setError(ErrorCode code, const std::string& message);
    void notifyStateChanged(DetectorState newState);
    void notifyError(const ErrorInfo& error);
    void notifyImageReceived(const ImageData& image);
    std::string stateToString(DetectorState state) const;

    // Error code mapping
    ErrorCode mapVarexError(VarexError varexError) const;
    DetectorState mapVarexState(VarexState varexState) const;
    AcquisitionParams convertVarexParams(const void* varexParams) const;
};

/**
 * @brief Synchronous acquisition interface for VarexDetector
 */
class VarexDetectorSynchronous : public IDetectorSynchronous {
public:
    explicit VarexDetectorSynchronous(VarexDetector* detector);
    virtual ~VarexDetectorSynchronous() = default;

    bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) override;
    bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) override;
    bool cancelAcquisition() override;

private:
    VarexDetector* detector_;
    std::atomic<bool> cancelled_;
};

} // namespace uxdi::adapters::varex
