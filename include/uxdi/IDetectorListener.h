#pragma once

#include "Types.h"

namespace uxdi {

/**
 * @brief Abstract callback interface for detector events
 *
 * IDetectorListener receives asynchronous notifications from IDetector.
 * Implement this interface to handle image data and state changes.
 */
class UXDI_API IDetectorListener {
public:
    virtual ~IDetectorListener() = default;

    // Image data callback (called during acquisition)
    virtual void onImageReceived(const ImageData& image) = 0;

    // State change callback
    virtual void onStateChanged(DetectorState newState) = 0;

    // Error callback
    virtual void onError(const ErrorInfo& error) = 0;

    // Acquisition started callback
    virtual void onAcquisitionStarted() = 0;

    // Acquisition stopped callback
    virtual void onAcquisitionStopped() = 0;
};

} // namespace uxdi
