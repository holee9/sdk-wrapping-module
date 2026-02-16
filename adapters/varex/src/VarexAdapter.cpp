#include "uxdi/IDetector.h"
#include "uxdi/uxdi_export.h"

// VarexAdapter implementation - Placeholder for Task 10
// Wraps Varex X-ray detector SDK

extern "C" {

UXDI_API uxdi::IDetector* CreateDetector(const char* config) {
    // Placeholder: Will return VarexAdapter instance
    (void)config;  // Suppress unused parameter warning
    return nullptr;
}

UXDI_API void DestroyDetector(uxdi::IDetector* detector) {
    // Placeholder: Will safely delete VarexAdapter instance
    if (detector) {
        delete detector;
    }
}

} // extern "C"
