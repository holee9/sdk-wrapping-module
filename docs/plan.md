Universal X-ray Detector Interface (UXDI) 개발을 위한 아키텍처 명세와 구현 가이드입니다. 벤더 종속성을 제거하고, 고성능(Zero-Copy) 및 의료기기 수준의 안정성을 보장하는 구조로 작성되었습니다.

Markdown
# Project UXDI: Universal X-ray Detector Interface Development Plan

## 1. Executive Summary
본 프로젝트는 다양한 X-ray Detector 제조사(Varex, Vieworks, Rayence, Samsung, DRTech, ABYZ 등)의 상이한 SDK를 단일 표준 인터페이스(`IDetector`)로 추상화하는 미들웨어 개발을 목표로 합니다. 또한, 실제 하드웨어 없이도 디텍터 동작을 에뮬레이션할 수 있는 가상 모듈(Emulator)을 제공합니다. 이를 통해 콘솔 애플리케이션(Console SW)은 하드웨어 변경 시에도 재컴파일 없이 플러그인 교체만으로 운영 가능해야 합니다.

**Core Objectives:**
1.  **Vendor Agnosticism:** 제조사 종속성 완전 제거 (Interface-based Design).
2.  **Zero-Copy Pipeline:** 16-bit 대용량 RAW 이미지의 불필요한 메모리 복사 방지.
3.  **Reliability:** SDK 크래시 및 예외 상황(케이블 분리 등)에 대한 격리 및 복구.

---

## 2. System Architecture

콘솔 SW는 오직 `UXDI Core`와 통신하며, 실제 하드웨어 제어는 런타임에 로드된 `Vendor Adapter`가 수행합니다.

```text
[ Console Application (GUI/Logic) ]
          |
          v (Call via Interface)
+-------------------------------------------------------+
|  UXDI Core Layer (Interface Definition & Manager)     |
|  - IDetector.h (Abstract Base Class)                  |
|  - Data Structures (ImageFrame, DetectorState)        |
|  - Plugin Factory (DLL Loader)                        |
+-------------------------------------------------------+
          | (Load Instance)
          +-----------------------------+
          |                             |
+----------------------+      +----------------------+      +----------------------+
|  Varex Adapter       |      |  Vieworks Adapter    |      |  ABYZ Adapter        |
|  (Implements UXDI)   |      |  (Implements UXDI)   |      |  (Implements UXDI)   |
+----------------------+      +----------------------+      +----------------------+
          |                             |                             |
    [ Vendor SDK A ]              [ Vendor SDK B ]              [ Vendor SDK C ]
          |                             |                             |
    ( Hardware A )                ( Hardware B )                ( Hardware C )

+----------------------+
|  Emulator (emul)     |
|  (Virtual Detector)  |
+----------------------+
          |
    [ No Hardware ]
    ( Emulated I/O )
3. Implementation Specification (C++20)
이 섹션의 코드는 인터페이스 정의, 데이터 구조, 그리고 어댑터 예시를 포함합니다.

3.1. Core Interface Definition (IDetector.h)
C++
/**
 * @file IDetector.h
 * @brief Universal X-ray Detector Interface Specification
 * @note Designed for ABI stability and High-Performance I/O
 */

#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace UXDI {

    // =========================================================
    // 1. Common Data Structures
    // =========================================================

    enum class DetectorState {
        Disconnected,
        Idle,           // Connected, Ready for commands
        Prep,           // Preparing for exposure (Anode spin-up, etc.)
        ReadyToFire,    // Window open, waiting for X-ray
        Acquiring,      // Reading out sensor data
        Processing,     // Internal correction
        Error
    };

    enum class TriggerMode {
        Software,       // App triggers acquisition
        AED,            // Auto Exposure Detection (Hardware trigger)
        ExternalSync    // Hardware cable sync
    };

    struct ImageFrame {
        uint32_t width;
        uint32_t height;
        uint32_t depth;       // 14 or 16 bit
        uint32_t frameId;
        uint64_t timestamp;   // Acquisition time
        std::shared_ptr<uint8_t[]> data; // Zero-copy buffer pointer
        size_t dataSize;
    };

    struct DetectorConfig {
        std::string configPath;   // Vendor specific config file (xml/ini)
        TriggerMode triggerMode;
        float exposureTimeMs;
        bool useInternalCorrection; // Offset/Gain/Defect
    };

    // =========================================================
    // 2. Event Listener (Observer Pattern)
    // =========================================================

    class IDetectorListener {
    public:
        virtual ~IDetectorListener() = default;
        virtual void OnStateChanged(DetectorState newState) = 0;
        virtual void OnImageReceived(const ImageFrame& image) = 0;
        virtual void OnError(int code, const std::string& message) = 0;
        virtual void OnTemperatureUpdate(float celsius) = 0;
    };

    // =========================================================
    // 3. The Abstract Interface (Contract)
    // =========================================================

    class IDetector {
    public:
        virtual ~IDetector() = default;

        // Lifecycle
        virtual bool Initialize(const DetectorConfig& config) = 0;
        virtual bool Connect() = 0;
        virtual void Disconnect() = 0;

        // Configuration
        virtual void SetListener(IDetectorListener* listener) = 0;
        virtual bool SetParameter(const std::string& key, float value) = 0;

        // Workflow Control
        virtual void Prepare() = 0;        // Move to 'Prep' state
        virtual void SoftwareTrigger() = 0; // Fire X-ray (if SW mode)
        virtual void Cancel() = 0;         // Abort current acquisition

        // Diagnostics
        virtual DetectorState GetState() const = 0;
    };

    // Factory Export Signature
    typedef IDetector* (*CreateDetectorFunc)();
    typedef void (*DestroyDetectorFunc)(IDetector*);
}
3.2. Concrete Adapter Implementation Example (VarexAdapter.cpp)
벤더의 SDK(여기서는 가상의 VarexSDK)를 어떻게 래핑하는지 보여주는 구현체입니다.

C++
/**
 * @file VarexAdapter.cpp
 * @brief Implementation of UXDI for Varex Panels
 */

#include "IDetector.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <iostream>

// Mock Varex SDK Headers (For demonstration)
namespace VarexSDK {
    typedef void (*ImageCallback)(void* pData, int w, int h);
    void RegisterCallback(ImageCallback cb);
    void Init();
    void SetTrigger(int mode);
    void Prep();
    void Fire();
}

class VarexAdapter : public UXDI::IDetector {
private:
    UXDI::IDetectorListener* m_listener = nullptr;
    UXDI::DetectorState m_currentState = UXDI::DetectorState::Disconnected;
    UXDI::DetectorConfig m_config;
    std::mutex m_mutex;

    // Internal State Tracking
    void UpdateState(UXDI::DetectorState newState) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentState = newState;
        if (m_listener) m_listener->OnStateChanged(newState);
    }

    // Static Callback Bridge (Vendor C-API -> C++ Class)
    static void SDK_ImageCallback_Bridge(void* pData, int w, int h) {
        // Warning: This is a static method, need instance context.
        // In production, use a singleton mapping or userdata pointer.
        // For this example, we assume access to the instance.
        VarexAdapter::Instance()->HandleImage(pData, w, h);
    }

public:
    static VarexAdapter* Instance() { static VarexAdapter inst; return &inst; }

    bool Initialize(const UXDI::DetectorConfig& config) override {
        m_config = config;
        try {
            VarexSDK::Init();
            VarexSDK::RegisterCallback(SDK_ImageCallback_Bridge);
            // Map configuration
            int sdkTrigger = (config.triggerMode == UXDI::TriggerMode::AED) ? 1 : 0;
            VarexSDK::SetTrigger(sdkTrigger);
            
            UpdateState(UXDI::DetectorState::Idle);
            return true;
        } catch (...) {
            if (m_listener) m_listener->OnError(500, "SDK Init Failed");
            return false;
        }
    }

    bool Connect() override {
        // HW Connection Logic...
        UpdateState(UXDI::DetectorState::Idle);
        return true;
    }

    void Disconnect() override {
        UpdateState(UXDI::DetectorState::Disconnected);
    }

    void SetListener(UXDI::IDetectorListener* listener) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_listener = listener;
    }

    bool SetParameter(const std::string& key, float value) override {
        // Generic Parameter Handling
        return true; 
    }

    void Prepare() override {
        UpdateState(UXDI::DetectorState::Prep);
        VarexSDK::Prep();
        // Assume immediate ready for demo
        UpdateState(UXDI::DetectorState::ReadyToFire);
    }

    void SoftwareTrigger() override {
        if (m_currentState != UXDI::DetectorState::ReadyToFire) {
             if (m_listener) m_listener->OnError(400, "Detector Not Ready");
             return;
        }
        UpdateState(UXDI::DetectorState::Acquiring);
        VarexSDK::Fire();
    }

    void Cancel() override {
        UpdateState(UXDI::DetectorState::Idle);
    }

    UXDI::DetectorState GetState() const override {
        return m_currentState;
    }

    // Core Logic: Image Handling
    void HandleImage(void* pRawData, int w, int h) {
        if (!m_listener) return;

        // 1. Wrap Data (Zero Copy strategy preferred)
        // Note: If SDK buffer is volatile, we MUST copy.
        // If SDK gives ownership, we wrap. Here assumes copy needed for safety.
        size_t size = w * h * sizeof(uint16_t);
        std::shared_ptr<uint8_t[]> buffer(new uint8_t[size]); 
        memcpy(buffer.get(), pRawData, size);

        UXDI::ImageFrame frame;
        frame.width = w;
        frame.height = h;
        frame.depth = 16;
        frame.data = buffer;
        frame.dataSize = size;
        frame.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        // 2. Dispatch to Listener
        m_listener->OnImageReceived(frame);
        
        // 3. Reset State
        UpdateState(UXDI::DetectorState::Idle);
    }
};

// Export Functions for DLL
extern "C" {
    __declspec(dllexport) UXDI::IDetector* CreateDetector() {
        return VarexAdapter::Instance();
    }
    __declspec(dllexport) void DestroyDetector(UXDI::IDetector* p) {
        // Singleton in this example, usually delete p;
    }
}
3.3. Factory & Manager (Console Side Logic)
C++
/**
 * @file UXDI_Manager.h
 * @brief Dynamic Plugin Loader
 */

#include "IDetector.h"
#include <windows.h> // Or <dlfcn.h> for Linux

class DetectorFactory {
public:
    static std::shared_ptr<UXDI::IDetector> LoadAdapter(const std::string& dllPath) {
        // 1. Load DLL
        HMODULE hDll = LoadLibraryA(dllPath.c_str());
        if (!hDll) throw std::runtime_error("Failed to load DLL");

        // 2. Get Function Pointers
        auto creator = (UXDI::CreateDetectorFunc)GetProcAddress(hDll, "CreateDetector");
        if (!creator) throw std::runtime_error("Invalid Wrapper DLL");

        // 3. Create Instance with Custom Deleter
        return std::shared_ptr<UXDI::IDetector>(creator(), [hDll](UXDI::IDetector* p) {
            // Cleanup logic would go here (DestroyFunc)
            // FreeLibrary(hDll); // Warning: careful with lifetime
        });
    }
};

4. Development Roadmap & Milestones
Phase 1: Core Framework (Weeks 1-2)
W1: IDetector.h 인터페이스 최종 확정 (모든 벤더 공통 분모 추출).

W2: Dummy Detector 어댑터 구현 (실제 하드웨어 없이 개발 가능한 시뮬레이터).

기능: 로컬 .raw 파일 로드, 타이머를 이용한 가상 노출 지연 시뮬레이션.

Phase 2: Pilot Implementation (Weeks 3-5)
W3: Target A (예: Varex) SDK 분석 및 1차 래핑.

초점: 초기화 시퀀스 및 Callback 연결.

W4: Target B (예: Vieworks) SDK 분석 및 1차 래핑.

초점: 비동기 Polling 방식 -> Event 방식 변환 로직 구현.

W4.5: Emulator (emul) 가상 디텍터 모듈 구현.

초점: 실제 디텍터 프로토콜 시뮬레이션, 다양한 시나리오(정상/오류/지연) 에뮬레이션.

W5: Target C (예: ABYZ) SDK 분석 및 1차 래핑 + 통합 테스트 (Memory Leak, Reconnection Stress Test).

Phase 3: Integration & Optimization (Weeks 6-7)
W6: Console GUI 연동 및 Zero-Copy 성능 검증.

W7: 예외 처리 강화 (케이블 발거, SDK 내부 에러 매핑).

5. Technical Constraints & Guidelines
ABI Safety: 인터페이스 클래스(IDetector)에는 멤버 변수를 두지 않으며, 오직 Pure Virtual Function만 사용해야 합니다.

Thread Safety: 어댑터 내부에서 발생하는 모든 SDK Callback은 std::mutex로 보호되어야 하며, 데이터 경쟁 상태(Race Condition)를 방지해야 합니다.

Memory Management: 이미지 데이터는 std::shared_ptr를 통해 관리되어, 콘솔이 처리를 끝낼 때까지 메모리가 유효함을 보장해야 합니다.

Error Code Mapping: 각 벤더의 고유 에러 코드는 반드시 UXDI::ErrorCode 열거형으로 변환되어 상위 레이어로 전달되어야 합니다.