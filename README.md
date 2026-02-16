# UXDI - Universal X-ray Detector Interface

<div align="center">

**Middleware for abstracting various X-ray Detector vendor SDKs behind a standard interface**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://img.shields.io/badge/build-passing-brightgreen)
[![Tests](https://img.shields.io/badge/tests-106%20passing-brightgreen)](https://img.shields.io/badge/tests-106%20passing-brightgreen)
[![C++](https://img.shields.io/badge/C%2B%2B20-00599C.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-Apache%25202.0-blue.svg)](LICENSE)

</div>

## Overview

UXDI (Universal X-ray Detector Interface) is a C++20 middleware that abstracts various X-ray Detector vendor SDKs (Varex, Vieworks, ABYZ, etc.) behind a standard `IDetector` interface. This allows console applications to switch between different detector hardware by simply replacing the adapter DLL, without recompilation.

### Key Features

- **Vendor Agnostic**: Interface-based design completely removes vendor dependencies
- **Zero-Copy Pipeline**: `shared_ptr<uint8_t[]>` for 16-bit RAW image data without unnecessary memory copies
- **Hot-Swappable Adapters**: Runtime DLL loading/unloading for hardware flexibility
- **Thread-Safe**: `std::mutex` protected state management
- **Production-Ready**: Comprehensive error handling and exception safety

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Console Application (GUI/Logic)               │
└─────────────────────────────────────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│  UXDI Core Layer (Static Library)                                 │
│  ┌──────────────┐  ┌───────────┐  ┌────────────┐              │
│  │ IDetector.h  │  │  Types.h  │  │  Factory   │              │
│  │ (Interface)  │  │ (Structs) │  │ (DLL Load) │              │
│  └──────────────┘  └───────────┘  └────────────┘              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              DetectorManager (Lifecycle Management)      │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                                 │
                    ┌────────────┼────────────┐
                    ▼            ▼            ▼
        ┌───────────┐  ┌──────────┐  ┌───────────┐
        │ Dummy     │  │  Emul    │  │ Varex     │
        │ Adapter   │  │ Adapter  │  │ Adapter   │
        └───────────┘  └──────────┘  └───────────┘
           │              │             │
        ┌───┴───┐      ┌───┴───┐     ┌───┴───┐
        │Mock │      │Scenario│     │Mock │
        │HW   │      │Engine│     │SDK  │
        └─────┘      └──────┘     └─────┘

```

## Project Status

| Component | Status | Description |
|-----------|--------|-------------|
| Core Framework | ✅ Complete | IDetector, Types, Factory, Manager |
| Virtual Adapters | ✅ Complete | DummyAdapter (test), EmulAdapter (scenario-based) |
| Vendor Adapters | ✅ Skeleton | Varex, Vieworks, ABYZ (with Mock SDKs) |
| Unit Tests | ✅ Passing | 106/106 tests |
| CLI Sample | ✅ Complete | Full-featured command-line interface |
| Build System | ✅ Complete | CMake 3.20+, MSVC 2022, C++20 |

## Building

### Requirements

- **Compiler**: MSVC 2022 (Visual Studio 17.x)
- **CMake**: Version 3.20 or higher
- **C++ Standard**: C++20

### Build Steps

```bash
# Configure (Windows)
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Build (Debug)
cmake --build build --config Debug

# Build (Release)
cmake --build build --config Release
```

### Build Artifacts

| Artifact | Location |
|----------|----------|
| Core Library | `build/lib/uxdi_core.lib` |
| Adapter DLLs | `build/bin/uxdi_*.dll` |
| CLI Executable | `build/bin/uxdi_cli.exe` |
| Test Executable | `build/bin/uxdi_core_tests.exe` |

## Usage

### CLI Sample Application

The `uxdi_cli.exe` provides a command-line interface for detector control:

```bash
# Run interactive demo (automated test sequence)
uxdi_cli.exe

# Show help
uxdi_cli.exe --help

# List loaded adapters
uxdi_cli.exe --list

# Load an adapter
uxdi_cli.exe --load uxdi_dummy.dll

# Create a detector
uxdi_cli.exe --create 1

# Show detector information
uxdi_cli.exe --info 1

# Start acquisition
uxdi_cli.exe --start 1

# Show detector state
uxdi_cli.exe --state 1

# Unload adapter
uxdi_cli.exe --unload 1
```

### Available Commands

| Command | Description |
|---------|-------------|
| `--list` | List loaded adapters |
| `--load <dll>` | Load adapter DLL |
| `--unload <id>` | Unload adapter |
| `--create <id>` | Create detector from adapter |
| `--destroy <id>` | Destroy detector |
| `--start <id>` | Start acquisition |
| `--state <id>` | Show detector state |
| `--info <id>` | Show detector information |
| `--params <id>` | Set acquisition parameters |
| `--detectors` | List managed detectors |
| `--help` | Show help message |

## Adapter List

| Adapter | DLL | Description |
|---------|-----|-------------|
| DummyAdapter | `uxdi_dummy.dll` | Test adapter with static black frames |
| EmulAdapter | `uxdi_emul.dll` | Scriptable test scenarios via ScenarioEngine |
| VarexAdapter | `uxdi_varex.dll` | Varex detector (Mock SDK) |
| VieworksAdapter | `uxdi_vieworks.dll` | Vieworks detector (Mock SDK) |
| ABYZAdapter | `uxdi_abyz.dll` | ABYZ detector (Skeleton) |

## Core Interfaces

### IDetector

The main interface that all adapters must implement:

```cpp
class UXDI_API IDetector {
public:
    virtual ~IDetector() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual bool shutdown() = 0;
    virtual bool isInitialized() const = 0;

    // Detector information
    virtual DetectorInfo getDetectorInfo() const = 0;
    virtual std::string getVendorName() const = 0;
    virtual std::string getModelName() const = 0;

    // State management
    virtual DetectorState getState() const = 0;
    virtual std::string getStateString() const = 0;

    // Configuration
    virtual bool setAcquisitionParams(const AcquisitionParams& params) = 0;
    virtual AcquisitionParams getAcquisitionParams() const = 0;

    // Listener management
    virtual void setListener(IDetectorListener* listener) = 0;
    virtual IDetectorListener* getListener() const = 0;

    // Asynchronous acquisition
    virtual bool startAcquisition() = 0;
    virtual bool stopAcquisition() = 0;
    virtual bool isAcquiring() const = 0;

    // Synchronous interface accessor
    virtual std::shared_ptr<IDetectorSynchronous> getSynchronousInterface() = 0;

    // Error handling
    virtual ErrorInfo getLastError() const = 0;
    virtual void clearError() = 0;
};
```

### Data Structures

```cpp
// Detector state enumeration
enum class DetectorState {
    UNKNOWN, IDLE, INITIALIZING, READY, ACQUIRING, STOPPING, ERROR
};

// Image data with zero-copy support
struct ImageData {
    uint32_t width;
    uint32_t height;
    uint32_t bitDepth;
    uint64_t frameNumber;
    double timestamp;
    std::shared_ptr<uint8_t[]> data;  // Zero-copy buffer
    size_t dataLength;
};

// Acquisition parameters
struct AcquisitionParams {
    uint32_t width;
    uint32_t height;
    uint32_t offsetX;
    uint32_t offsetY;
    float exposureTimeMs;
    float gain;
    uint32_t binning;
};
```

## Testing

### Run Unit Tests

```bash
cd build/bin
uxdi_core_tests.exe
```

### Test Results

```
[==========] 106 tests from 3 test suites ran. (14 ms total)
[  PASSED  ] 106 tests.

[----------] 25 tests from DetectorTypes
[----------] 40 tests from DetectorFactoryTest
[----------] 41 tests from DetectorManagerTest
```

## Directory Structure

```
sdk-wrapping-module/
├── include/uxdi/          # Core interface headers
│   ├── IDetector.h
│   ├── IDetectorListener.h
│   ├── IDetectorSynchronous.h
│   ├── Types.h
│   ├── DetectorFactory.h
│   ├── DetectorManager.h
│   └── uxdi_export.h
├── src/uxdi/               # Core framework implementation
│   ├── DetectorFactory.cpp
│   └── DetectorManager.cpp
├── adapters/               # Adapter DLLs
│   ├── dummy/              # Dummy adapter (testing)
│   ├── emul/               # Emulator adapter (scenario-based)
│   ├── varex/              # Varex adapter (mock SDK)
│   ├── vieworks/           # Vieworks adapter (mock SDK)
│   └── ├── abz/              # ABYZ adapter (skeleton)
├── tests/                   # Unit and integration tests
│   └── test_core/
│   └── integration/
├── examples/cli/            # CLI sample application
├── cmake/                   # CMake modules
├── CMakeLists.txt           # Root CMake configuration
└── build/                   # Build output directory
```

## Development Roadmap

### Phase 1: Core Framework ✅
- [x] IDetector interface definition
- [x] Data structures (Types.h)
- [x] DetectorFactory (DLL loading)
- [x] DetectorManager (lifecycle management)
- [x] Unit tests

### Phase 2: Adapters ✅
- [x] DummyAdapter (test simulator)
- [x] EmulAdapter (ScenarioEngine)
- [x] VarexAdapter (mock SDK)
- [x] VieworksAdapter (mock SDK)
- [x] ABYZAdapter (skeleton)

### Phase 3: Integration ✅
- [x] CLI sample application
- [x] Integration tests
- [x] Zero-copy pipeline verification
- [x] Thread safety validation

## Technical Constraints

### ABI Safety
- Interface classes (`IDetector`, `IDetectorListener`) have **no member variables**
- Only pure virtual functions are used for ABI stability

### Thread Safety
- All adapter callbacks are protected with `std::mutex`
- State management is race-condition free

### Memory Management
- Image data uses `std::shared_ptr<uint8_t[]>` for zero-copy
- Memory remains valid until console finishes processing

### Error Handling
- All vendor-specific error codes are mapped to `ErrorCode` enum
- Exception-safe with proper cleanup on errors

## License

Apache License 2.0

## Contributing

This project is currently in active development. For contribution guidelines, please refer to the project documentation.

## Authors

- UXDI Development Team

## Version

Current Version: 0.1.0
