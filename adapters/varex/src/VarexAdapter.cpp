#include "VarexDetector.h"

using namespace uxdi::adapters::varex;

//=============================================================================
// DLL Export Functions
//=============================================================================

// Export macros for adapter DLL
#ifdef UXDI_VAREX_EXPORTS
#define ADAPTER_API __declspec(dllexport)
#else
#define ADAPTER_API __declspec(dllimport)
#endif

extern "C" {

/**
 * @brief Create a new VarexDetector instance
 *
 * This function is called by DetectorFactory to load the Varex adapter.
 * The config parameter is currently unused but reserved for future configuration.
 *
 * @param config JSON configuration string (can be empty or null)
 * @return Pointer to IDetector interface, or nullptr on failure
 */
ADAPTER_API uxdi::IDetector* CreateDetector(const char* config) {
    (void)config;  // Config is reserved for future use

    try {
        auto* detector = new VarexDetector();

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
 * @brief Destroy a VarexDetector instance
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
