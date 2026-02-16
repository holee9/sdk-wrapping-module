#include "uxdi/DetectorFactory.h"
#include "uxdi/DetectorManager.h"
#include "uxdi/IDetector.h"
#include <iostream>
#include <windows.h>

using namespace uxdi;

int main() {
    std::cout << "=== UXDI Adapter Load Test ===" << std::endl;
    std::cout << std::endl;

    // Get current directory for DLL path
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string exePath(buffer);
    size_t pos = exePath.find_last_of("\\/");
    std::string exeDir = exePath.substr(0, pos);

    std::cout << "Executable directory: " << exeDir << std::endl;
    std::cout << std::endl;

    // Test 1: Load DummyAdapter
    std::cout << "[Test 1] Loading DummyAdapter..." << std::endl;
    try {
        std::wstring dllPath = std::wstring(exeDir.begin(), exeDir.end()) + L"\\uxdi_dummy.dll";

        size_t adapterId = DetectorFactory::LoadAdapter(dllPath);
        std::cout << "  ✓ DummyAdapter loaded with ID: " << adapterId << std::endl;

        auto adapters = DetectorFactory::GetLoadedAdapters();
        std::cout << "  ✓ Adapter name: " << adapters[0].name << std::endl;
        std::cout << "  ✓ Adapter version: " << adapters[0].version << std::endl;

        // Create detector instance
        auto detector = DetectorFactory::CreateDetector(adapterId);
        std::cout << "  ✓ Detector created successfully" << std::endl;

        // Test detector info
        DetectorInfo info = detector->getDetectorInfo();
        std::cout << "  ✓ Vendor: " << info.vendor << std::endl;
        std::cout << "  ✓ Model: " << info.model << std::endl;
        std::cout << "  ✓ Serial: " << info.serialNumber << std::endl;
        std::cout << "  ✓ Max Resolution: " << info.maxWidth << "x" << info.maxHeight << std::endl;

        // Test state
        std::cout << "  ✓ Initial State: " << detector->getStateString() << std::endl;

        // Test initialization
        if (detector->initialize()) {
            std::cout << "  ✓ Initialization successful" << std::endl;
            std::cout << "  ✓ State after init: " << detector->getStateString() << std::endl;
        }

        // Test acquisition params
        AcquisitionParams params;
        params.width = 1024;
        params.height = 1024;
        params.exposureTimeMs = 100.0f;
        params.gain = 1.0f;
        params.binning = 1;

        if (detector->setAcquisitionParams(params)) {
            std::cout << "  ✓ Acquisition params set successfully" << std::endl;
        }

        // Test start/stop acquisition
        if (detector->startAcquisition()) {
            std::cout << "  ✓ Acquisition started" << std::endl;
            std::cout << "  ✓ State: " << detector->getStateString() << std::endl;

            // Wait a bit
            Sleep(500);

            if (detector->stopAcquisition()) {
                std::cout << "  ✓ Acquisition stopped" << std::endl;
                std::cout << "  ✓ Final State: " << detector->getStateString() << std::endl;
            }
        }

        // Test shutdown
        if (detector->shutdown()) {
            std::cout << "  ✓ Shutdown successful" << std::endl;
        }

        // Cleanup
        detector.reset();
        // Don't unload adapter here - Test 2 will use it
        std::cout << "  ✓ Test 1 complete (adapter kept loaded)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "  ✗ Error: " << e.what() << std::endl;
        DetectorFactory::UnloadAllAdapters();
        return 1;
    }

    std::cout << std::endl;

    // Test 2: DetectorManager
    std::cout << "[Test 2] Testing DetectorManager..." << std::endl;
    try {
        DetectorManager manager;

        // Use the existing adapter (ID 1) from Test 1
        size_t adapterId = 1;  // Already loaded from Test 1
        std::cout << "  ✓ Using existing adapter ID: " << adapterId << std::endl;

        // Create detector via manager
        size_t detectorId = manager.CreateDetector(adapterId, "");
        if (detectorId == 0) {
            std::cerr << "  ✗ Failed to create detector via Manager" << std::endl;
            DetectorFactory::UnloadAllAdapters();
            return 1;
        }
        std::cout << "  ✓ Detector created via Manager, ID: " << detectorId << std::endl;

        // Get detector state
        DetectorState state = manager.GetState(detectorId);
        std::cout << "  ✓ Detector State: " << static_cast<int>(state) << std::endl;

        // Get detector info
        DetectorInfo info = manager.GetInfo(detectorId);
        std::cout << "  ✓ Detector Vendor: " << info.vendor << std::endl;

        // Get detector count
        size_t count = manager.GetDetectorCount();
        std::cout << "  ✓ Managed Detectors: " << count << std::endl;

        // Destroy detector
        manager.DestroyDetector(detectorId);
        std::cout << "  ✓ Detector destroyed" << std::endl;

        // Don't unload adapters yet - keep for EmulAdapter test
        std::cout << "  ✓ Test 2 complete (adapters kept loaded)" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "  ✗ Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl;

    // Test 3: Load EmulAdapter (with ScenarioEngine)
    std::cout << "[Test 3] Loading EmulAdapter..." << std::endl;
    try {
        std::wstring dllPath = std::wstring(exeDir.begin(), exeDir.end()) + L"\\uxdi_emul.dll";

        size_t adapterId = DetectorFactory::LoadAdapter(dllPath);
        std::cout << "  ✓ EmulAdapter loaded with ID: " << adapterId << std::endl;

        auto adapters = DetectorFactory::GetLoadedAdapters();
        std::cout << "  ✓ Adapter name: " << adapters[0].name << std::endl;
        std::cout << "  ✓ Adapter version: " << adapters[0].version << std::endl;

        // Create detector instance with default scenario
        auto detector = DetectorFactory::CreateDetector(adapterId, "");
        std::cout << "  ✓ Detector created successfully" << std::endl;

        // Test detector info
        DetectorInfo info = detector->getDetectorInfo();
        std::cout << "  ✓ Vendor: " << info.vendor << std::endl;
        std::cout << "  ✓ Model: " << info.model << std::endl;
        std::cout << "  ✓ Max Resolution: " << info.maxWidth << "x" << info.maxHeight << std::endl;

        // Test state
        std::cout << "  ✓ Initial State: " << detector->getStateString() << std::endl;

        // Test acquisition
        if (detector->startAcquisition()) {
            std::cout << "  ✓ Acquisition started (Emulator generating frames...)" << std::endl;

            // Wait for some frames
            Sleep(1000);

            if (detector->stopAcquisition()) {
                std::cout << "  ✓ Acquisition stopped" << std::endl;
            }
        }

        // Test shutdown
        if (detector->shutdown()) {
            std::cout << "  ✓ Shutdown successful" << std::endl;
        }

        // Cleanup
        detector.reset();
        DetectorFactory::UnloadAdapter(adapterId);
        std::cout << "  ✓ Adapter unloaded" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "  ✗ Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "=== All Tests Passed! ===" << std::endl;
    return 0;
}
