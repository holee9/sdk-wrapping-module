#include "uxdi/IDetector.h"
#include "uxdi/uxdi_export.h"

// VieworksAdapter implementation - Placeholder for Task 11
// Wraps Vieworks X-ray detector SDK

extern "C" {

UXDI_API uxdi::IDetector* CreateDetector(const char* config) {
    // Placeholder: Will return VieworksAdapter instance
    (void)config;  // Suppress unused parameter warning
    return nullptr;
}

UXDI_API void DestroyDetector(uxdi::IDetector* detector) {
    // Placeholder: Will safely delete VieworksAdapter instance
    if (detector) {
        delete detector;
    }
}

} // extern "C"
