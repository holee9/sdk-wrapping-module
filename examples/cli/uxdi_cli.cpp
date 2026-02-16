#include <iostream>
#include <iomanip>
#include "uxdi/DetectorFactory.h"
#include "uxdi/DetectorManager.h"
#include "uxdi/IDetector.h"
#include "uxdi/Types.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <codecvt>
#include <locale>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

using namespace uxdi;

// CLI Color codes for Windows
#ifdef _WIN32
void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}
const int COLOR_RESET = 7;   // White
const int COLOR_GREEN = 10;  // Green
const int COLOR_CYAN = 11;   // Cyan
const int COLOR_YELLOW = 14; // Yellow
const int COLOR_RED = 12;    // Red
#else
const int COLOR_RESET = 0;
const int COLOR_GREEN = 32;
const int COLOR_CYAN = 36;
const int COLOR_YELLOW = 33;
const int COLOR_RED = 31;
void SetColor(int) {}
#endif

// UTF-8 to Wide String conversion for Windows
#ifdef _WIN32
std::wstring ToWideString(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
#endif

// Print header
void PrintHeader() {
    SetColor(COLOR_CYAN);
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║          UXDI - Universal X-ray Detector Interface           ║
║                    CLI Sample Application                     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
    SetColor(COLOR_RESET);
    std::cout << "Version: 0.1.0" << std::endl;
    std::cout << std::endl;
}

// Print section header
void PrintSection(const std::string& title) {
    SetColor(COLOR_YELLOW);
    std::cout << "\n>>> " << title << " <<<" << std::endl;
    SetColor(COLOR_RESET);
}

// Print success message
void PrintSuccess(const std::string& message) {
    SetColor(COLOR_GREEN);
    std::cout << "[✓] " << message << std::endl;
    SetColor(COLOR_RESET);
}

// Print error message
void PrintError(const std::string& message) {
    SetColor(COLOR_RED);
    std::cout << "[✗] " << message << std::endl;
    SetColor(COLOR_RESET);
}

// Print info message
void PrintInfo(const std::string& message) {
    SetColor(COLOR_CYAN);
    std::cout << "[i] " << message << std::endl;
    SetColor(COLOR_RESET);
}

// List loaded adapters
void ListAdapters() {
    PrintSection("Loaded Adapters");

    auto adapters = DetectorFactory::GetLoadedAdapters();
    if (adapters.empty()) {
        PrintInfo("No adapters loaded");
        return;
    }

    std::cout << std::left << std::setw(5) << "ID"
              << std::setw(20) << "Name"
              << std::setw(12) << "Version"
              << "Description" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (size_t i = 0; i < adapters.size(); ++i) {
        size_t id = i + 1;  // Position-based ID
        std::cout << std::left << std::setw(5) << id
                  << std::setw(20) << adapters[i].name
                  << std::setw(12) << adapters[i].version
                  << adapters[i].description << std::endl;
    }
}

// Load adapter
size_t LoadAdapter(const std::string& dllPath) {
    PrintSection("Loading Adapter");
    PrintInfo("DLL: " + dllPath);

    try {
#ifdef _WIN32
        std::wstring wDllPath = ToWideString(dllPath);
        size_t adapterId = DetectorFactory::LoadAdapter(wDllPath);
#else
        size_t adapterId = DetectorFactory::LoadAdapter(dllPath);
#endif
        PrintSuccess("Adapter loaded with ID: " + std::to_string(adapterId));
        return adapterId;
    } catch (const std::exception& e) {
        PrintError(std::string("Failed to load adapter: ") + e.what());
        return 0;
    }
}

// Unload adapter
void UnloadAdapter(size_t adapterId) {
    PrintSection("Unloading Adapter");
    PrintInfo("Adapter ID: " + std::to_string(adapterId));

    try {
        DetectorFactory::UnloadAdapter(adapterId);
        PrintSuccess("Adapter unloaded");
    } catch (const std::exception& e) {
        PrintError(std::string("Failed to unload adapter: ") + e.what());
    }
}

// List detectors managed by DetectorManager
void ListDetectors(DetectorManager& manager) {
    PrintSection("Managed Detectors");

    auto detectorIds = manager.GetDetectorIds();
    if (detectorIds.empty()) {
        PrintInfo("No detectors created");
        return;
    }

    for (size_t detectorId : detectorIds) {
        DetectorState state = manager.GetState(detectorId);
        DetectorInfo info = manager.GetInfo(detectorId);

        std::cout << "Detector ID: " << detectorId << std::endl;
        std::cout << "  Vendor: " << info.vendor << std::endl;
        std::cout << "  Model: " << info.model << std::endl;
        std::cout << "  State: " << static_cast<int>(state) << std::endl;
        std::cout << std::endl;
    }
}

// Create detector via DetectorManager
size_t CreateDetector(DetectorManager& manager, size_t adapterId, const std::string& config) {
    PrintSection("Creating Detector");
    PrintInfo("Adapter ID: " + std::to_string(adapterId));
    PrintInfo("Config: " + (config.empty() ? "(default)" : config));

    size_t detectorId = manager.CreateDetector(adapterId, config);
    if (detectorId == 0) {
        PrintError("Failed to create detector");
        return 0;
    }

    PrintSuccess("Detector created with ID: " + std::to_string(detectorId));

    // Show detector info
    DetectorInfo info = manager.GetInfo(detectorId);
    std::cout << "  Vendor: " << info.vendor << std::endl;
    std::cout << "  Model: " << info.model << std::endl;
    std::cout << "  Serial: " << info.serialNumber << std::endl;
    std::cout << "  Resolution: " << info.maxWidth << "x" << info.maxHeight << std::endl;

    return detectorId;
}

// Destroy detector
void DestroyDetector(DetectorManager& manager, size_t detectorId) {
    PrintSection("Destroying Detector");
    PrintInfo("Detector ID: " + std::to_string(detectorId));

    manager.DestroyDetector(detectorId);
    PrintSuccess("Detector destroyed");
}

// Start acquisition
void StartAcquisition(DetectorManager& manager, size_t detectorId) {
    PrintSection("Starting Acquisition");
    PrintInfo("Detector ID: " + std::to_string(detectorId));

    IDetector* detector = manager.GetDetector(detectorId);
    if (!detector) {
        PrintError("Detector not found");
        return;
    }

    if (detector->startAcquisition()) {
        PrintSuccess("Acquisition started");
        PrintInfo("State: " + detector->getStateString());
        PrintInfo("Press Enter to stop acquisition...");

        // Wait for user input to stop
        std::cin.get();

        detector->stopAcquisition();
        PrintSuccess("Acquisition stopped");
        PrintInfo("Final State: " + detector->getStateString());
    } else {
        PrintError("Failed to start acquisition");
    }
}

// Show detector state
void ShowDetectorState(DetectorManager& manager, size_t detectorId) {
    PrintSection("Detector State");
    PrintInfo("Detector ID: " + std::to_string(detectorId));

    IDetector* detector = manager.GetDetector(detectorId);
    if (!detector) {
        PrintError("Detector not found");
        return;
    }

    DetectorState state = detector->getState();
    std::cout << "  State: " << detector->getStateString() << std::endl;
    std::cout << "  Value: " << static_cast<int>(state) << std::endl;
}

// Show detector info
void ShowDetectorInfo(DetectorManager& manager, size_t detectorId) {
    PrintSection("Detector Information");
    PrintInfo("Detector ID: " + std::to_string(detectorId));

    IDetector* detector = manager.GetDetector(detectorId);
    if (!detector) {
        PrintError("Detector not found");
        return;
    }

    DetectorInfo info = detector->getDetectorInfo();
    std::cout << "  Vendor: " << info.vendor << std::endl;
    std::cout << "  Model: " << info.model << std::endl;
    std::cout << "  Serial: " << info.serialNumber << std::endl;
    std::cout << "  Firmware: " << info.firmwareVersion << std::endl;
    std::cout << "  Max Width: " << info.maxWidth << std::endl;
    std::cout << "  Max Height: " << info.maxHeight << std::endl;
    std::cout << "  Bit Depth: " << info.bitDepth << std::endl;
}

// Set acquisition parameters
void SetAcquisitionParams(DetectorManager& manager, size_t detectorId) {
    PrintSection("Setting Acquisition Parameters");
    PrintInfo("Detector ID: " + std::to_string(detectorId));

    IDetector* detector = manager.GetDetector(detectorId);
    if (!detector) {
        PrintError("Detector not found");
        return;
    }

    AcquisitionParams params = detector->getAcquisitionParams();

    std::cout << "\nCurrent Parameters:" << std::endl;
    std::cout << "  Width: " << params.width << std::endl;
    std::cout << "  Height: " << params.height << std::endl;
    std::cout << "  Offset X: " << params.offsetX << std::endl;
    std::cout << "  Offset Y: " << params.offsetY << std::endl;
    std::cout << "  Exposure: " << params.exposureTimeMs << " ms" << std::endl;
    std::cout << "  Gain: " << params.gain << std::endl;
    std::cout << "  Binning: " << params.binning << std::endl;

    // Set new parameters (example)
    params.width = 1024;
    params.height = 1024;
    params.exposureTimeMs = 100.0f;
    params.gain = 1.0f;
    params.binning = 1;

    if (detector->setAcquisitionParams(params)) {
        PrintSuccess("Parameters updated");
    } else {
        PrintError("Failed to set parameters");
    }
}

// Print usage
void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [command] [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  --list                    List loaded adapters" << std::endl;
    std::cout << "  --load <dll_path>         Load adapter DLL" << std::endl;
    std::cout << "  --unload <adapter_id>      Unload adapter" << std::endl;
    std::cout << "  --create <adapter_id>      Create detector from adapter" << std::endl;
    std::cout << "  --destroy <detector_id>    Destroy detector" << std::endl;
    std::cout << "  --start <detector_id>      Start acquisition" << std::endl;
    std::cout << "  --stop <detector_id>       Stop acquisition" << std::endl;
    std::cout << "  --state <detector_id>      Show detector state" << std::endl;
    std::cout << "  --info <detector_id>       Show detector information" << std::endl;
    std::cout << "  --params <detector_id>     Set acquisition parameters" << std::endl;
    std::cout << "  --detectors               List managed detectors" << std::endl;
    std::cout << "  --help                    Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " --list" << std::endl;
    std::cout << "  " << programName << " --load uxdi_dummy.dll" << std::endl;
    std::cout << "  " << programName << " --create 1" << std::endl;
    std::cout << "  " << programName << " --start 1" << std::endl;
}

// Interactive demo mode
void RunInteractiveDemo() {
    PrintHeader();
    PrintInfo("Running interactive demo mode...");

    DetectorManager manager;

    // Step 1: Load DummyAdapter
    size_t adapterId = LoadAdapter("uxdi_dummy.dll");
    if (adapterId == 0) {
        PrintError("Cannot proceed without adapter");
        return;
    }

    // Step 2: Show loaded adapters
    ListAdapters();

    // Step 3: Create detector
    size_t detectorId = CreateDetector(manager, adapterId, "");
    if (detectorId == 0) {
        PrintError("Cannot proceed without detector");
        DetectorFactory::UnloadAllAdapters();
        return;
    }

    // Step 4: Show detector info
    ShowDetectorInfo(manager, detectorId);

    // Step 5: Set parameters
    SetAcquisitionParams(manager, detectorId);

    // Step 6: Show state
    ShowDetectorState(manager, detectorId);

    // Step 7: Start/Stop acquisition
    PrintSection("Acquisition Test");
    PrintInfo("Starting acquisition for 2 seconds...");

    IDetector* detector = manager.GetDetector(detectorId);
    if (detector && detector->startAcquisition()) {
        PrintSuccess("Acquisition started");

#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif

        detector->stopAcquisition();
        PrintSuccess("Acquisition stopped");
    }

    // Step 8: Show final state
    ShowDetectorState(manager, detectorId);

    // Step 9: Cleanup
    PrintSection("Cleanup");
    manager.DestroyDetector(detectorId);
    DetectorFactory::UnloadAllAdapters();
    PrintSuccess("Cleanup complete");

    PrintSection("Demo Complete");
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
}

int main(int argc, char* argv[]) {
    // Set UTF-8 console for Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // No arguments - run interactive demo
    if (argc == 1) {
        RunInteractiveDemo();
        return 0;
    }

    // Parse command line arguments
    std::string command = argv[1];
    DetectorManager manager;

    if (command == "--help" || command == "-h") {
        PrintUsage(argv[0]);
        return 0;
    }
    else if (command == "--list") {
        ListAdapters();
    }
    else if (command == "--load") {
        if (argc < 3) {
            PrintError("Usage: --load <dll_path>");
            return 1;
        }
        LoadAdapter(argv[2]);
    }
    else if (command == "--unload") {
        if (argc < 3) {
            PrintError("Usage: --unload <adapter_id>");
            return 1;
        }
        UnloadAdapter(std::stoul(argv[2], nullptr, 10));
    }
    else if (command == "--detectors") {
        ListDetectors(manager);
    }
    else if (command == "--create") {
        if (argc < 3) {
            PrintError("Usage: --create <adapter_id> [config]");
            return 1;
        }
        size_t adapterId = std::stoul(argv[2], nullptr, 10);
        std::string config = (argc >= 4) ? argv[3] : "";
        CreateDetector(manager, adapterId, config);
    }
    else if (command == "--destroy") {
        if (argc < 3) {
            PrintError("Usage: --destroy <detector_id>");
            return 1;
        }
        DestroyDetector(manager, std::stoul(argv[2], nullptr, 10));
    }
    else if (command == "--start") {
        if (argc < 3) {
            PrintError("Usage: --start <detector_id>");
            return 1;
        }
        StartAcquisition(manager, std::stoul(argv[2], nullptr, 10));
    }
    else if (command == "--state") {
        if (argc < 3) {
            PrintError("Usage: --state <detector_id>");
            return 1;
        }
        ShowDetectorState(manager, std::stoul(argv[2], nullptr, 10));
    }
    else if (command == "--info") {
        if (argc < 3) {
            PrintError("Usage: --info <detector_id>");
            return 1;
        }
        ShowDetectorInfo(manager, std::stoul(argv[2], nullptr, 10));
    }
    else if (command == "--params") {
        if (argc < 3) {
            PrintError("Usage: --params <detector_id>");
            return 1;
        }
        SetAcquisitionParams(manager, std::stoul(argv[2], nullptr, 10));
    }
    else {
        PrintError("Unknown command: " + command);
        std::cout << "Use --help for usage information" << std::endl;
        return 1;
    }

    return 0;
}
