# UXDI - Universal X-ray Detector Interface

<div align="center">

**Middleware for abstracting various X-ray Detector vendor SDKs behind a standard interface**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://img.shields.io/badge/build-passing-brightgreen)
[![Tests](https://img.shields.io/badge/tests-106%20passing-brightgreen)](https://img.shields.io/badge/tests-106%20passing-brightgreen)
[![C++](https://img.shields.io/badge/C%2B%2B20-00599C.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-Apache%25202.0-blue.svg)](LICENSE)

</div>

---

## Table of Contents

- [Overview](#overview)
- [Features](#key-features)
- [Architecture](#architecture)
- [Project Status](#project-status)
- [GUI Demo](#gui-demo)
- [Building](#building)
- [Usage](#usage)
- [Adapters](#adapter-list)
- [API Reference](#core-interfaces)
- [Testing](#testing)
- [Directory Structure](#directory-structure)
- [Development Roadmap](#development-roadmap)
- [Claude Code + Codex MCP Integration](#claude-code--codex-mcp-integration)

---

## Overview

UXDI (Universal X-ray Detector Interface) is a C++20 middleware that abstracts various X-ray Detector vendor SDKs (Varex, Vieworks, ABYZ, etc.) behind a standard `IDetector` interface. This allows applications to switch between different detector hardware by simply replacing the adapter DLL, without recompilation.

## Key Features

- **Vendor Agnostic**: Interface-based design completely removes vendor dependencies
- **Zero-Copy Pipeline**: `shared_ptr<uint8_t[]>` for 16-bit RAW image data without unnecessary memory copies
- **Hot-Swappable Adapters**: Runtime DLL loading/unloading for hardware flexibility
- **Thread-Safe**: `std::mutex` protected state management
- **Production-Ready**: Comprehensive error handling and exception safety
- **GUI Demo**: Desktop application with Dear ImGui for visual testing and debugging

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │
│  │   GUI Demo   │  │  CLI Sample  │  │  Your Application    │ │
│  │ (Dear ImGui) │  │   (uxdi_cli) │  │                      │ │
│  └──────────────┘  └──────────────┘  └──────────────────────┘ │
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
| **GUI Demo** | ✅ Complete | **Desktop application with Dear ImGui** |
| Build System | ✅ Complete | CMake 3.20+, MSVC 2022, C++20 |

---

## GUI Demo

The GUI Demo (`gui_demo.exe`) is a desktop application built with Dear ImGui and DirectX 11 that provides a visual interface for testing and debugging UXDI functionality.

### Features

- **Visual Panel Layout**: Organized panels for adapters, control, status, and image display
- **Real-time Image Display**: View acquired X-ray images with FPS graph overlay
- **Interactive Controls**: Click-to-load adapters, initialize detectors, start/stop acquisition
- **Status Monitoring**: Real-time detector state, frame count, and FPS statistics
- **Error Display**: User-friendly error messages with clear action buttons
- **Keyboard Shortcuts**: F1 (Help), F5 (Acquisition), ESC (Exit)

### Screenshot

```
┌─────────────────────────────────────────────────────────────────────┐
│ File │ View │                                                        │ Menu Bar
├──────┴──────┴────────────────────────────────────────────────────────┤
│ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────────────┐ │
│ │  Adapters   │ │   Control   │ │           Status                 │ │
│ │             │ │             │ │                                 │ │
│ │ [Select ▼]  │ │ State: READY│ │ Vendor: UXDI                    │ │
│ │ [Load]      │ │ [Init]      │ │ Model: DUMMY-001                │ │
│ │             │ │ [Start Acq] │ │                                 │ │
│ │ Loaded:     │ │             │ │ Frames: 1,234                   │ │
│ │ • Dummy     │ │ [Create]    │ │ FPS: 30.5                       │ │
│ │ • Emul      │ │             │ │                                 │ │
│ │ • Varex     │ │             │ │ Keyboard: F1=Help F5=Acq ESC=Exit│ │
│ └─────────────┘ └─────────────┘ └─────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────────┐ │
│ │                     Image Display                                 │ │
│ │  ┌───────────────────────────────────────────────────────────┐  │ │
│ │  │                                                           │  │ │
│ │  │                    [X-Ray Image]                          │  │ │
│ │  │                                                           │  │ │
│ │  │                                                           │  │ │
│ │  └───────────────────────────────────────────────────────────┘  │ │
│ │  Frame: 1234 | 1024x768 | 16-bit  │  FPS Graph: ▂▄▆█▅▃       │ │
│ └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### Running the GUI Demo

```bash
# Navigate to build output
cd build/bin

# Run the GUI demo
gui_demo.exe
```

### GUI Demo Panels

| Panel | Description |
|-------|-------------|
| **Menu Bar** | File (Exit), View (Window toggles) |
| **Adapters** | Select and load detector adapter DLLs |
| **Detector Control** | Initialize detector, start/stop acquisition |
| **Status** | Detector info, frame count, FPS, errors |
| **Image Display** | Live X-ray image viewer with FPS graph |

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `F1` | Show/Hide Help window |
| `F5` | Start/Stop acquisition (when detector is ready) |
| `ESC` | Exit application (with confirmation) |

### GUI Demo Technical Details

- **Rendering**: DirectX 11 with Dear ImGui
- **Font Loading**: Automatic font atlas generation
- **Blend State**: Alpha blending for transparent UI elements
- **Thread Safety**: Mutex-protected frame data sharing between detector callbacks and UI
- **Memory**: Zero-copy image data pipeline using `shared_ptr<uint8_t[]>`

---

## Building

### Requirements

- **Compiler**: MSVC 2022 (Visual Studio 17.x)
- **CMake**: Version 3.20 or higher
- **C++ Standard**: C++20
- **Dear ImGui**: Fetched automatically via CMake
- **DirectX 11**: Windows SDK

### Build Steps

```bash
# Configure (Windows)
cmake -B build -S . -G "Visual Studio 17 2022" -A x64

# Build (Debug)
cmake --build build --config Debug

# Build (Release)
cmake --build build --config Release

# Build GUI demo only
cmake --build build --target gui_demo --config Debug
```

### Build Artifacts

| Artifact | Location |
|----------|----------|
| Core Library | `build/lib/uxdi_core.lib` |
| Adapter DLLs | `build/bin/uxdi_*.dll` |
| CLI Executable | `build/bin/uxdi_cli.exe` |
| **GUI Executable** | `build/bin/gui_demo.exe` |
| Test Executable | `build/bin/uxdi_core_tests.exe` |
| ImGui Library | `build/lib/imgui_lib.lib` |

---

## Usage

### GUI Demo Application

```bash
# Run the GUI demo
cd build/bin
gui_demo.exe

# Using command prompt
start gui_demo.exe

# Using PowerShell
Start-Process gui_demo.exe
```

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

### CLI Commands

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

---

## Adapter List

| Adapter | DLL | Description | Status |
|---------|-----|-------------|--------|
| DummyAdapter | `uxdi_dummy.dll` | Test adapter with static black frames | ✅ Complete |
| EmulAdapter | `uxdi_emul.dll` | Scriptable test scenarios via ScenarioEngine | ✅ Complete |
| VarexAdapter | `uxdi_varex.dll` | Varex detector (Mock SDK) | ✅ Skeleton |
| VieworksAdapter | `uxdi_vieworks.dll` | Vieworks detector (Mock SDK) | ✅ Skeleton |
| ABYZAdapter | `uxdi_abyz.dll` | ABYZ detector (Skeleton) | ✅ Skeleton |

---

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

---

## Testing

### Run Unit Tests

```bash
cd build/bin
uxdi_core_tests.exe
```

### Run Integration Tests

```bash
cd build/bin
test_adapter_load.exe
```

### Test Results

```
[==========] 106 tests from 3 test suites ran. (14 ms total)
[  PASSED  ] 106 tests.

[----------] 25 tests from DetectorTypes
[----------] 40 tests from DetectorFactoryTest
[----------] 41 tests from DetectorManagerTest
```

---

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
│   └── abyz/               # ABYZ adapter (skeleton)
├── mock_sdk/               # Mock SDK implementations
│   ├── varex/              # Varex Mock SDK
│   ├── vieworks/           # Vieworks Mock SDK
│   └── abyz/               # ABYZ Mock SDK
├── tests/                  # Unit and integration tests
│   └── test_core/
│   └── integration/
├── examples/
│   ├── cli/                # CLI sample application
│   └── gui_demo/           # GUI demo application (Dear ImGui)
│       ├── main.cpp
│       ├── GUIDemoApp.h
│       ├── GUIDemoApp.cpp
│       └── CMakeLists.txt
├── cmake/                  # CMake modules
├── CMakeLists.txt          # Root CMake configuration
└── build/                  # Build output directory
```

---

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
- [x] **GUI demo application (Dear ImGui)**
- [x] Integration tests
- [x] Zero-copy pipeline verification
- [x] Thread safety validation

### Phase 4: Production (Future)
- [ ] Real vendor SDK integration
- [ ] Performance optimization
- [ ] Production testing
- [ ] Documentation completion
- [ ] CI/CD pipeline

---

## Technical Constraints

### ABI Safety
- Interface classes (`IDetector`, `IDetectorListener`) have **no member variables**
- Only pure virtual functions are used for ABI stability

### Thread Safety
- All adapter callbacks are protected with `std::mutex`
- State management is race-condition free

### Memory Management
- Image data uses `std::shared_ptr<uint8_t[]>` for zero-copy
- Memory remains valid until application finishes processing

### Error Handling
- All vendor-specific error codes are mapped to `ErrorCode` enum
- Exception-safe with proper cleanup on errors

---

## Claude Code + Codex MCP Integration

This project has verified and tested integration between Claude Code and OpenAI's Codex extension for AI-powered task delegation.

### Quick Setup

**Prerequisites:**
- OpenAI ChatGPT VSCode extension (`openai.chatgpt`) installed
- ChatGPT account logged in (Plus, Pro, Business, Edu, or Enterprise plan)

**Configuration (`C:\Users\user\.mcp.json`):**

```json
{
  "$schema": "https://raw.githubusercontent.com/anthropics/claude-code/main/.mcp.schema.json",
  "mcpServers": {
    "codex": {
      "$comment": "OpenAI Codex - AI task delegation via MCP",
      "command": "C:\\Users\\user\\.vscode\\extensions\\openai.chatgpt-0.4.74-win32-x64\\bin\\windows-x86_64\\codex.exe",
      "args": ["mcp-server"]
    }
  },
  "staggeredStartup": {
    "enabled": true,
    "delayMs": 500,
    "connectionTimeout": 60000
  }
}
```

**Claude Code Settings (`settings.json`):**

```json
{
  "permissions": {
    "mcp__codex*": "allow"
  }
}
```

**Verify Setup:**

```bash
# Check Codex login status
"C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64\bin\windows-x86_64\codex.exe" login status
```

Expected output: `Logged in using ChatGPT`

### Available Tools

| Tool | Purpose |
|------|---------|
| `mcp__codex__codex` | Start new conversation |
| `mcp__codex__codex-reply` | Continue conversation with threadId |

### Verified Use Cases

- ✅ Mathematical calculations
- ✅ TypeScript/JavaScript code generation
- ✅ Next.js/React component creation
- ✅ Multi-turn conversations with context preservation

<details>
<summary>View Detailed Integration Guide</summary>

### Installation Paths

| Component | Path |
|-----------|------|
| Codex Extension | `C:\Users\user\.vscode\extensions\openai.chatgpt-0.4.74-win32-x64` |
| Codex Executable | `bin\windows-x86_64\codex.exe` |
| MCP Config | `C:\Users\user\.mcp.json` (global) |

### Usage Examples

**Basic Calculation:**
```javascript
mcp__codex__codex({ prompt: "Calculate: 15 * 23 + 7" })
// Result: 352
```

**TypeScript Code Generation:**
```javascript
mcp__codex__codex({
  prompt: "TypeScript로 피보나치 수열을 계산하는 함수를 작성해주세요."
})
```

**Multi-turn Conversation:**
```javascript
// Start conversation
const result1 = mcp__codex__codex({ prompt: "What is 2+2?" })

// Continue with threadId
const result2 = mcp__codex__codex-reply({
  threadId: result1.threadId,
  prompt: "Now write a TypeScript function for fibonacci"
})
```

### Troubleshooting

| Symptom | Solution |
|---------|----------|
| MCP tools not loading | Increase `connectionTimeout` to 60000ms |
| "Not logged in" error | Run `codex login` again |
| Tools fail to appear | Add `mcp__codex*` to settings.json permissions |

### Verification Checklist

- [ ] Codex extension installed in VSCode
- [ ] `codex login status` shows "Logged in"
- [ ] `.mcp.json` configured with correct codex.exe path
- [ ] `connectionTimeout` set to 60000ms
- [ ] Claude Code restarted after configuration

</details>

---

## License

Apache License 2.0

## Contributing

This project is currently in active development. For contribution guidelines, please refer to the project documentation.

## Authors

- UXDI Development Team

## Version

Current Version: 0.2.0
