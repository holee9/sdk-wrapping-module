#pragma once

#include "Types.h"
#include <memory>
#include <vector>

namespace uxdi {

/**
 * @brief Synchronous acquisition interface
 *
 * IDetectorSynchronous provides blocking image acquisition operations
 * for applications that prefer synchronous control flow.
 */
class UXDI_API IDetectorSynchronous {
public:
    virtual ~IDetectorSynchronous() = default;

    // Synchronous single-frame acquisition
    virtual bool acquireFrame(ImageData& outImage, uint32_t timeoutMs = 5000) = 0;

    // Synchronous multi-frame acquisition
    virtual bool acquireFrames(uint32_t frameCount, std::vector<ImageData>& outImages, uint32_t timeoutMs = 30000) = 0;

    // Cancel ongoing acquisition
    virtual bool cancelAcquisition() = 0;
};

} // namespace uxdi
