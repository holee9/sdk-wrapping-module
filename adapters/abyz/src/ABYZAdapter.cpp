#include "ABYZDetector.h"

using namespace uxdi::adapters::abyz;

//=============================================================================
// DLL Export Functions
//=============================================================================

// Export macros for adapter DLL
#ifdef UXDI_ABYZ_EXPORTS
#define ADAPTER_API __declspec(dllexport)
#else
#define ADAPTER_API __declspec(dllimport)
#endif

extern "C" {

/**
 * @brief Create a new ABYZDetector instance
 *
 * This function is called by DetectorFactory to load the ABYZ adapter.
 * The config parameter specifies the vendor (Rayence, Samsung, or DRTech).
 *
 * Config format examples:
 * - {"vendor": "rayence"}  - Rayence detector
 * - {"vendor": "samsung"}  - Samsung detector
 * - {"vendor": "drtech"}   - DRTech detector
 * - "" or null             - Default (Rayence)
 *
 * @param config JSON configuration string with "vendor" field
 * @return Pointer to IDetector interface, or nullptr on failure
 */
ADAPTER_API uxdi::IDetector* CreateDetector(const char* config) {
    // Pass config to detector for vendor selection
    std::string configStr = config ? config : "";

    try {
        auto* detector = new ABYZDetector(configStr);

        // Auto-initialize for convenience
        if (!detector->initialize()) {
            delete detector;
            return nullptr;
        }

        return detector;
    } catch (...) {
        return nullptr;
    }
}

/**
 * @brief Destroy an ABYZDetector instance
 *
 * This function safely cleans up a detector created by CreateDetector.
 *
 * @param detector Pointer to IDetector interface to destroy
 */
ADAPTER_API void DestroyDetector(uxdi::IDetector* detector) {
    if (detector) {
        try {
            // Ensure shutdown is called before deletion
            if (detector->isInitialized()) {
                detector->shutdown();
            }
            delete detector;
        } catch (...) {
            // Suppress exceptions during destructor
            delete detector;
        }
    }
}

} // extern "C"
