#pragma once

#include <uxdi/IDetector.h>
#include <uxdi/uxdi_export.h>
#include <uxdi/Types.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace uxdi {

// Forward declarations
class IDetector;

// Factory function pointers from adapter DLLs
using CreateDetectorFunc = IDetector* (*)(const char* config);
using DestroyDetectorFunc = void (*)(IDetector* detector);

// Custom deleter for IDetector that calls adapter's DestroyDetector
struct DetectorFactoryDeleter {
    DestroyDetectorFunc destroyFunc = nullptr;
    size_t adapterId = 0;

    void operator()(IDetector* detector) const;
};

// Detector adapter descriptor
struct DetectorAdapterInfo {
    std::string name;           // e.g., "DummyAdapter"
    std::string version;        // Adapter version string
    std::string description;    // Adapter description
    std::string dllPath;        // Path to the DLL
};

/**
 * @brief Factory for dynamically loading and managing detector adapter DLLs
 *
 * DetectorFactory provides a mechanism to load adapter DLLs at runtime,
 * create detector instances, and manage their lifecycle. Thread-safe operations
 * are supported for concurrent access.
 */
class UXDI_API DetectorFactory {
public:
    /**
     * @brief Load an adapter DLL from the specified path
     *
     * Loads a DLL and verifies it exports the required CreateDetector and
     * DestroyDetector functions.
     *
     * @param dllPath Wide-character path to the adapter DLL
     * @return Adapter ID for later reference in CreateDetector/UnloadAdapter
     * @throws std::runtime_error on failure (DLL not found, missing exports, etc.)
     */
    static size_t LoadAdapter(const std::wstring& dllPath);

    /**
     * @brief Get information about all loaded adapters
     *
     * Thread-safe accessor to the list of currently loaded adapters.
     *
     * @return Vector of DetectorAdapterInfo structures
     */
    static std::vector<DetectorAdapterInfo> GetLoadedAdapters();

    /**
     * @brief Create a detector instance from the specified adapter
     *
     * Creates a new detector instance using the adapter's CreateDetector export.
     * The returned unique_ptr uses a custom deleter that calls the adapter's
     * DestroyDetector function.
     *
     * @param adapterId ID returned from LoadAdapter
     * @param config JSON configuration string for the detector (may be empty)
     * @return unique_ptr to the created IDetector instance
     * @throws std::runtime_error if adapterId is invalid or creation fails
     */
    static std::unique_ptr<IDetector, DetectorFactoryDeleter> CreateDetector(
        size_t adapterId,
        const std::string& config = ""
    );

    /**
     * @brief Destroy a detector instance via its adapter
     *
     * Calls the adapter's DestroyDetector function. This is automatically
     * called by unique_ptr when the detector goes out of scope, but can
     * be called explicitly for early cleanup.
     *
     * @param detector Pointer to the detector to destroy (will be null after call)
     * @throws std::runtime_error if detector is null or adapter not found
     */
    static void DestroyDetector(std::unique_ptr<IDetector, DetectorFactoryDeleter>& detector);

    /**
     * @brief Unload a specific adapter DLL
     *
     * Releases the DLL handle. Any detectors created from this adapter
     * should be destroyed before unloading.
     *
     * @param adapterId ID returned from LoadAdapter
     * @throws std::runtime_error if adapterId is invalid
     */
    static void UnloadAdapter(size_t adapterId);

    /**
     * @brief Unload all adapter DLLs
     *
     * Convenience method to release all loaded DLLs.
     */
    static void UnloadAllAdapters();

    /**
     * @brief Convert UTF-8 string to wide string (UTF-16)
     *
     * Utility function for converting paths for Windows API calls.
     *
     * @param utf8 UTF-8 encoded string
     * @return Wide string (UTF-16)
     */
    static std::wstring ToWideString(const std::string& utf8);

    /**
     * @brief Convert wide string (UTF-16) to UTF-8
     *
     * Utility function for converting Windows API paths to UTF-8.
     *
     * @param wide Wide string (UTF-16)
     * @return UTF-8 encoded string
     */
    static std::string ToUtf8String(const std::wstring& wide);

private:
    // Internal handle structure for loaded adapters
    struct AdapterHandle {
        HMODULE hModule;
        CreateDetectorFunc createFunc;
        DestroyDetectorFunc destroyFunc;
        DetectorAdapterInfo info;
        std::wstring dllPath;
    };

    // Static storage for loaded adapters
    static std::vector<AdapterHandle> s_loadedAdapters;
    static std::mutex s_mutex;
    static size_t s_nextAdapterId;
};

} // namespace uxdi
