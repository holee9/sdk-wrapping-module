#include "uxdi/DetectorFactory.h"
#include <stdexcept>
#include <system_error>
#include <vector>

namespace uxdi {

// Static member initialization
std::vector<DetectorFactory::AdapterHandle> DetectorFactory::s_loadedAdapters;
std::mutex DetectorFactory::s_mutex;
size_t DetectorFactory::s_nextAdapterId = 1;

// Custom deleter implementation
void DetectorFactoryDeleter::operator()(IDetector* detector) const {
    if (detector && destroyFunc) {
        destroyFunc(detector);
    }
}

size_t DetectorFactory::LoadAdapter(const std::wstring& dllPath) {
    std::lock_guard<std::mutex> lock(s_mutex);

    // Load the DLL
    HMODULE hModule = LoadLibraryW(dllPath.c_str());
    if (!hModule) {
        DWORD error = GetLastError();
        throw std::runtime_error(
            "Failed to load DLL: " + ToUtf8String(dllPath) +
            " (Error code: " + std::to_string(error) + ")"
        );
    }

    // Get CreateDetector function
    auto createFunc = reinterpret_cast<CreateDetectorFunc>(
        GetProcAddress(hModule, "CreateDetector")
    );
    if (!createFunc) {
        FreeLibrary(hModule);
        throw std::runtime_error(
            "DLL does not export CreateDetector: " + ToUtf8String(dllPath)
        );
    }

    // Get DestroyDetector function
    auto destroyFunc = reinterpret_cast<DestroyDetectorFunc>(
        GetProcAddress(hModule, "DestroyDetector")
    );
    if (!destroyFunc) {
        FreeLibrary(hModule);
        throw std::runtime_error(
            "DLL does not export DestroyDetector: " + ToUtf8String(dllPath)
        );
    }

    // Try to query adapter info by creating a temporary detector
    // (Note: This assumes adapters can be created with empty config to query info)
    DetectorAdapterInfo info;
    info.dllPath = ToUtf8String(dllPath);
    info.name = "Unknown";
    info.version = "1.0.0";
    info.description = "Detector Adapter";

    // Extract adapter name from DLL filename
    std::wstring filename = dllPath;
    size_t lastSlash = filename.find_last_of(L"/\\");
    if (lastSlash != std::wstring::npos) {
        filename = filename.substr(lastSlash + 1);
    }
    size_t lastDot = filename.find_last_of(L'.');
    if (lastDot != std::wstring::npos) {
        filename = filename.substr(0, lastDot);
    }
    info.name = ToUtf8String(filename);

    // Store the adapter handle
    AdapterHandle handle;
    handle.hModule = hModule;
    handle.createFunc = createFunc;
    handle.destroyFunc = destroyFunc;
    handle.info = info;
    handle.dllPath = dllPath;

    size_t adapterId = s_nextAdapterId++;
    s_loadedAdapters.push_back(std::move(handle));

    return adapterId;
}

std::vector<DetectorAdapterInfo> DetectorFactory::GetLoadedAdapters() {
    std::lock_guard<std::mutex> lock(s_mutex);

    std::vector<DetectorAdapterInfo> result;
    result.reserve(s_loadedAdapters.size());

    for (const auto& handle : s_loadedAdapters) {
        result.push_back(handle.info);
    }

    return result;
}

std::unique_ptr<IDetector, DetectorFactoryDeleter> DetectorFactory::CreateDetector(
    size_t adapterId,
    const std::string& config
) {
    // Find the adapter by ID
    CreateDetectorFunc createFunc = nullptr;
    DestroyDetectorFunc destroyFunc = nullptr;

    {
        std::lock_guard<std::mutex> lock(s_mutex);

        if (adapterId == 0 || adapterId >= s_nextAdapterId) {
            throw std::runtime_error(
                "Invalid adapter ID: " + std::to_string(adapterId)
            );
        }

        for (const auto& handle : s_loadedAdapters) {
            // Match by position-based ID (index + 1)
            size_t currentId = (&handle - &s_loadedAdapters[0]) + 1;
            if (currentId == adapterId) {
                createFunc = handle.createFunc;
                destroyFunc = handle.destroyFunc;
                break;
            }
        }

        if (!createFunc || !destroyFunc) {
            throw std::runtime_error(
                "Adapter not found or already unloaded: " + std::to_string(adapterId)
            );
        }
    }

    // Create the detector instance
    IDetector* detector = createFunc(config.c_str());
    if (!detector) {
        throw std::runtime_error(
            "CreateDetector returned nullptr for adapter " + std::to_string(adapterId)
        );
    }

    // Return unique_ptr with custom deleter
    return std::unique_ptr<IDetector, DetectorFactoryDeleter>(
        detector,
        DetectorFactoryDeleter{destroyFunc, adapterId}
    );
}

void DetectorFactory::DestroyDetector(std::unique_ptr<IDetector, DetectorFactoryDeleter>& detector) {
    if (detector) {
        // The unique_ptr deleter will handle calling DestroyDetector
        detector.reset();
    }
}

void DetectorFactory::UnloadAdapter(size_t adapterId) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (adapterId == 0 || adapterId >= s_nextAdapterId) {
        throw std::runtime_error(
            "Invalid adapter ID: " + std::to_string(adapterId)
        );
    }

    // Find and remove the adapter
    for (auto it = s_loadedAdapters.begin(); it != s_loadedAdapters.end(); ++it) {
        size_t currentId = (it - s_loadedAdapters.begin()) + 1;
        if (currentId == adapterId) {
            FreeLibrary(it->hModule);
            s_loadedAdapters.erase(it);
            return;
        }
    }

    throw std::runtime_error(
        "Adapter not found: " + std::to_string(adapterId)
    );
}

void DetectorFactory::UnloadAllAdapters() {
    std::lock_guard<std::mutex> lock(s_mutex);

    for (auto& handle : s_loadedAdapters) {
        FreeLibrary(handle.hModule);
    }

    s_loadedAdapters.clear();
}

std::wstring DetectorFactory::ToWideString(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }

    // Calculate required buffer size
    int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        utf8.c_str(),
        -1,
        nullptr,
        0
    );

    if (size <= 0) {
        throw std::runtime_error(
            "Failed to convert UTF-8 to wide string"
        );
    }

    // Convert the string
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        utf8.c_str(),
        -1,
        &result[0],
        size
    );

    return result;
}

std::string DetectorFactory::ToUtf8String(const std::wstring& wide) {
    if (wide.empty()) {
        return std::string();
    }

    // Calculate required buffer size
    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) {
        throw std::runtime_error(
            "Failed to convert wide string to UTF-8"
        );
    }

    // Convert the string
    std::string result(size - 1, '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wide.c_str(),
        -1,
        &result[0],
        size,
        nullptr,
        nullptr
    );

    return result;
}

} // namespace uxdi
