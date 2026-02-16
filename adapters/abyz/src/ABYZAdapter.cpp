#include "uxdi/IDetector.h"
#include "uxdi/uxdi_export.h"

// ABYZAdapter implementation - Placeholder for Task 12
// Wraps ABYZ (Rayence/Samsung/DRTech) X-ray detector SDKs

extern "C" {

UXDI_API uxdi::IDetector* CreateDetector(const char* config) {
    // Placeholder: Will return ABYZAdapter instance
    (void)config;  // Suppress unused parameter warning
    return nullptr;
}

UXDI_API void DestroyDetector(uxdi::IDetector* detector) {
    // Placeholder: Will safely delete ABYZAdapter instance
    if (detector) {
        delete detector;
    }
}

} // extern "C"
