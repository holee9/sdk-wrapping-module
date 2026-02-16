#pragma once

#include "Types.h"
#include <string>
#include <memory>

namespace uxdi {

/**
 * @brief Abstract interface for X-ray detector control
 *
 * IDetector defines the standard interface for all X-ray detector adapters.
 * This pure virtual interface ensures ABI stability across DLL boundaries.
 */
class UXDI_API IDetector {
public:
    virtual ~IDetector() = default;

    // Initialization and cleanup
    virtual bool initialize() = 0;
    virtual bool shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // Detector information
    virtual DetectorInfo getDetectorInfo() const = 0;
    virtual std::string getVendorName() const = 0;
    virtual std::string getModelName() const = 0;

    // State management
    virtual DetectorState getState() const = 0;
    virtual std::string getStateString() const = 0;

    // Configuration
    virtual bool setAcquisitionParams(const AcquisitionParams& params) = 0;
    virtual AcquisitionParams getAcquisitionParams() const = 0;

    // Listener management
    virtual void setListener(IDetectorListener* listener) = 0;
    virtual IDetectorListener* getListener() const = 0;

    // Asynchronous acquisition
    virtual bool startAcquisition() = 0;
    virtual bool stopAcquisition() = 0;
    virtual bool isAcquiring() const = 0;

    // Synchronous interface accessor
    virtual std::shared_ptr<IDetectorSynchronous> getSynchronousInterface() = 0;

    // Error handling
    virtual ErrorInfo getLastError() const = 0;
    virtual void clearError() = 0;
};

} // namespace uxdi
