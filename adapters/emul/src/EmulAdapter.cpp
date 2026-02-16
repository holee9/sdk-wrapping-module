#include "EmulDetector.h"
#include <cstring>

// When building the DLL, we need to export these functions
// When using the DLL, we need to import them
#ifdef UXDI_EMUL_EXPORTS
#define EMUL_API __declspec(dllexport)
#else
#define EMUL_API __declspec(dllimport)
#endif

using namespace uxdi::adapters::emul;

//=============================================================================
// DLL Export Functions
//=============================================================================

extern "C" {

/**
 * @brief Create a new EmulDetector instance
 *
 * This function is called by DetectorFactory to load the emulator adapter.
 * The config parameter supports multiple formats:
 * - Inline JSON scenario: {"scenario": {"name": "Test", "actions": [...]}}
 * - File reference: {"scenario_file": "path/to/scenario.json"} or "file://path"
 * - Empty string: Uses built-in default scenario
 *
 * @param config JSON configuration string (inline scenario, file reference, or empty)
 * @return Pointer to IDetector interface, or nullptr on failure
 */
EMUL_API uxdi::IDetector* CreateDetector(const char* config) {
    try {
        // Convert config to string (empty if nullptr)
        std::string configStr = (config != nullptr) ? config : "";

        auto* detector = new EmulDetector(configStr);

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
 * @brief Destroy an EmulDetector instance
 *
 * This function safely cleans up a detector created by CreateDetector.
 *
 * @param detector Pointer to IDetector interface to destroy
 */
EMUL_API void DestroyDetector(uxdi::IDetector* detector) {
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
