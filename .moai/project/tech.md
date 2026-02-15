# UXDI 기술 스택 및 개발 환경

## 1. 기술 스택 개요

### 핵심 기술

UXDI (Universal X-ray Detector Interface)는 다양한 X-ray Detector 제조사의 SDK를 표준 인터페이스로 추상화하는 C++20 기반 미들웨어입니다.

**프로그래밍 언어**: C++20 표준
- 모던 C++ 기능 활용 (공통 메모리 관리, 이동 의미론, constexpr 등)
- ABI 안정성을 위한 신중한 설계 필요
- 크로스 플랫폼 호환성 고려

**빌드 시스템**: CMake 3.20 이상
- Core 정적 라이브러리 (libuxdi_core.a)
- 어댑터 동적 라이브러리 (DLL 플러그인)
- 테스트 빌드 타겟 (CTest 통합)
- Install 타겟 (헤더 및 라이브러리 배포)

**플랫폼**: Windows (DLL 기반 플러그인 시스템)
- LoadLibrary/GetProcAddress를 통한 런타임 로딩
- DLL Export 함수를 통한 인스턴스 생성
- 제조사별 독립적인 어댑터 DLL

**테스트 프레임워크**: Google Test (GTest)
- 단위 테스트 (Unit Tests)
- 통합 테스트 (Integration Tests)
- Mock 객체를 통한 격리된 테스트

**패키지 관리**: vcpkg 또는 Conan
- 제3자 라이브러리 의존성 관리
- 버전 관리 및 호환성 확보
- 크로스 플랫폼 지원

---

## 2. 핵심 기술 결정

### 추상 기본 클래스 (Pure Virtual Interfaces)

**목적**: ABI 안정성 확보 및 벤더 중립성 유지

```
IDetector (Abstract Base Class)
├── Pure Virtual Methods 만 포함
├── 멤버 변수 절대 금지
├── 가상 소멸자 필수
└── 인터페이스 진화 시 호환성 보장
```

**제약사항**:
- IDetector 인터페이스는 순수 가상 함수만 포함
- 구현 클래스에서만 멤버 변수 선언
- 인터페이스 변경 시 호환성 유지 필수

### DLL 플러그인 시스템

**동작 원리**:
- 런타임에 어댑터 DLL 동적 로딩 (LoadLibrary)
- 팩토리 함수를 통한 인스턴스 생성 (GetProcAddress)
- 제조사별로 독립적인 DLL 파일로 배포

**장점**:
- 제조사 SDK와 메인 애플리케이션 분리
- 특정 벤더 SDK 크래시로부터 격리
- 선택적 어댑터 로딩 가능

**구현 패턴**:
```cpp
// Adapter.dll에서 Export
extern "C" {
  IDetector* CreateDetectorInstance();
  void DestroyDetectorInstance(IDetector* ptr);
}

// Core에서 로딩
HMODULE hLib = LoadLibrary(L"VarexAdapter.dll");
typedef IDetector* (*CreateFn)();
CreateFn createDetector = (CreateFn)GetProcAddress(hLib, "CreateDetectorInstance");
```

### 옵저버 패턴 (Observer Pattern)

**비동기 이벤트 전달**:
- IDetectorListener 인터페이스를 통한 콜백
- 이미지 수신, 상태 변경, 에러 발생 등의 이벤트
- 스레드 안전한 메시지 큐 기반 전달

**리스너 등록**:
```
IDetector::AttachListener(IDetectorListener* listener)
IDetector::DetachListener(IDetectorListener* listener)
```

**콜백 메서드**:
```
onImageReady(ImageFrame& frame)
onStateChanged(DetectorState newState)
onError(ErrorCode code, const char* message)
```

### shared_ptr 기반 메모리 관리

**Zero-Copy 이미지 파이프라인**:
- ImageFrame 데이터는 shared_ptr로 관리
- 소유권 이전 시 포인터만 복사 (메모리 복사 X)
- 마지막 참조 제거 시 자동 해제

**메모리 생명주기**:
```
Detector SDK
    |
    v (큰 이미지 데이터)
std::shared_ptr<ImageFrame>
    |
    ├─> Listener 1
    ├─> Listener 2
    └─> Listener 3

(모든 리스너 처리 후 자동 해제)
```

**장점**:
- 16-bit 대용량 RAW 이미지에서 불필요한 메모리 복사 방지
- 순환 참조 없이 자동 메모리 해제
- 원자적 참조 카운팅

### std::mutex 기반 스레드 안전성

**SDK 콜백 보호**:
- 모든 공유 데이터는 std::mutex로 보호
- 콜백 함수 내에서 뮤텍스 점유 최소화
- Callback Bridge에서 안전한 동기화 보장

**임계 영역 보호**:
```
SDK Callback (벤더 스레드에서 호출)
    |
    v (Lock Acquire)
std::lock_guard<std::mutex> lock(callback_mutex)
    |
    v (안전한 데이터 업데이트)
onImageReady(ImageFrame&)
    |
    v (Lock Release)
다음 콜백 가능
```

---

## 3. 개발 환경 요구사항

### 컴파일러

**Windows - MSVC 2022 (권장)**:
- Visual Studio 2022 Community/Professional
- C++20 표준 완전 지원
- /std:c++latest 또는 /std:c++20 플래그

**크로스 플랫폼 지원 (향후)**:
- GCC 12 이상 (Linux)
- Clang 15 이상 (macOS, Linux)

### IDE 및 도구

**Visual Studio 2022**:
- CMake 통합 지원
- C++20 IntelliSense
- 자동 테스트 실행기

**VS Code (대안)**:
- C/C++ 확장 (ms-vscode.cpptools)
- CMake Tools 확장
- OpenAI ChatGPT 확장 (선택사항)

### 빌드 도구

**CMake**: 3.20 이상
- 크로스 플랫폼 빌드 설정
- 플러그인 아키텍처 지원
- CTest 통합

### 코드 분석

**LSP (Language Server Protocol)**:
- clangd: 실시간 코드 분석, 자동완성, 타입 검사
- 인텔리센스 기능 제공
- C++20 문법 검증

**린터 및 포매터**:
- clang-format: C++ 코드 스타일 통일
- clang-tidy: 정적 분석 및 버그 감지

### 벤더 SDK 통합

각 제조사별 SDK 필요:
- **Varex**: Varex XRpad SDK
- **Vieworks**: Vieworks VDT SDK
- **Rayence**: Rayence SDK
- **Samsung**: Samsung S-Detect SDK
- **DRTech**: DRTech SDK
- **ABYZ**: ABYZ SDK

각 SDK는 라이선스 및 NDA에 따라 별도로 획득해야 합니다.

---

## 4. 빌드 설정

### CMake 프로젝트 구조

```
CMakeLists.txt (루트)
├── src/
│   ├── CMakeLists.txt (Core 라이브러리)
│   ├── uxdi/ (Core 헤더)
│   │   ├── IDetector.h
│   │   ├── DetectorConfig.h
│   │   ├── ImageFrame.h
│   │   └── ErrorCode.h
│   ├── core/ (Core 구현)
│   │   ├── DetectorManager.cpp
│   │   ├── PluginFactory.cpp
│   │   └── CallbackBridge.cpp
│   └── adapters/ (어댑터 플러그인)
│       ├── VarexAdapter/
│       │   ├── CMakeLists.txt
│       │   ├── VarexAdapter.cpp
│       │   └── VarexDetector.cpp
│       ├── VieworksAdapter/
│       │   └── ...
│       └── DummyAdapter/
│           └── ...
└── tests/
    ├── CMakeLists.txt (테스트 타겟)
    ├── unit/ (단위 테스트)
    └── integration/ (통합 테스트)
```

### Core 정적 라이브러리 설정

Core 라이브러리는 다음을 포함합니다:
- IDetector 인터페이스 정의
- DetectorManager (플러그인 로더)
- PluginFactory (동적 인스턴스 생성)
- 공통 데이터 구조 (ImageFrame, DetectorState 등)
- 콜백 브릿지 (C API → C++ 변환)

**설정 예시**:
```
add_library(uxdi_core STATIC
    src/core/DetectorManager.cpp
    src/core/PluginFactory.cpp
    src/core/CallbackBridge.cpp
)
target_include_directories(uxdi_core PUBLIC src/uxdi)
target_compile_features(uxdi_core PRIVATE cxx_std_20)
```

### 어댑터 DLL 빌드 타겟

각 어댑터는 독립적인 DLL로 빌드됩니다:

**Varex Adapter**:
```
add_library(VarexAdapter SHARED
    src/adapters/VarexAdapter/VarexAdapter.cpp
    src/adapters/VarexAdapter/VarexDetector.cpp
)
target_link_libraries(VarexAdapter PRIVATE uxdi_core ${VAREX_SDK_LIBS})
```

**Vieworks Adapter**:
```
add_library(VieworksAdapter SHARED
    src/adapters/VieworksAdapter/VieworksAdapter.cpp
    src/adapters/VieworksAdapter/VieworksDetector.cpp
)
target_link_libraries(VieworksAdapter PRIVATE uxdi_core ${VIEWORKS_SDK_LIBS})
```

**ABYZ Adapter**:
```
add_library(ABYZAdapter SHARED
    src/adapters/ABYZAdapter/ABYZAdapter.cpp
    src/adapters/ABYZAdapter/ABYZDetector.cpp
)
target_link_libraries(ABYZAdapter PRIVATE uxdi_core ${ABYZ_SDK_LIBS})
```

**Dummy Adapter** (테스트/시뮬레이션):
```
add_library(DummyAdapter SHARED
    src/adapters/DummyAdapter/DummyAdapter.cpp
    src/adapters/DummyAdapter/DummyDetector.cpp
)
target_link_libraries(DummyAdapter PRIVATE uxdi_core)
```

**Emulator (emul)** (가상 디텍터 에뮬레이터):
```
add_library(EmulAdapter SHARED
    src/adapters/EmulAdapter/EmulAdapter.cpp
    src/adapters/EmulAdapter/EmulDetector.cpp
    src/adapters/EmulAdapter/ScenarioEngine.cpp
)
target_link_libraries(EmulAdapter PRIVATE uxdi_core)
```

### 테스트 빌드 타겟 (CTest 통합)

```
enable_testing()

add_executable(unit_tests
    tests/unit/test_detector_interface.cpp
    tests/unit/test_plugin_factory.cpp
    tests/unit/test_memory_management.cpp
)
target_link_libraries(unit_tests PRIVATE uxdi_core gtest gtest_main)

add_test(NAME UnitTests COMMAND unit_tests)

add_executable(integration_tests
    tests/integration/test_adapter_lifecycle.cpp
    tests/integration/test_zero_copy_pipeline.cpp
)
target_link_libraries(integration_tests PRIVATE uxdi_core DummyAdapter gtest gtest_main)

add_test(NAME IntegrationTests COMMAND integration_tests)
```

### Install 타겟

```
install(DIRECTORY src/uxdi/ DESTINATION include/uxdi)
install(TARGETS uxdi_core DESTINATION lib)
install(TARGETS VarexAdapter VieworksAdapter ABYZAdapter DummyAdapter EmulAdapter DESTINATION lib/adapters)
```

---

## 5. 기술 제약사항

### ABI 안정성

**제약**: IDetector 인터페이스는 멤버 변수를 가질 수 없습니다.

**이유**:
- 멤버 변수 추가 시 바이너리 호환성 깨짐
- 이미 배포된 어댑터 DLL이 작동 불능
- 벤더별 독립적 업데이트 불가능

**검증 방법**:
```cpp
// ❌ 잘못된 예
class IDetector {
public:
    virtual void Expose() = 0;
    int frame_count;  // 멤버 변수 금지!
};

// ✅ 올바른 예
class IDetector {
public:
    virtual void Expose() = 0;
    virtual int GetFrameCount() = 0;  // Pure Virtual Method
};
```

### 스레드 안전성

**제약**: 모든 SDK 콜백은 std::mutex로 보호되어야 합니다.

**이유**:
- 벤더 SDK는 별도 스레드에서 콜백 호출
- 경합 조건 (Race Condition) 방지 필수
- 데이터 무결성 보장

**구현 패턴**:
```cpp
// Callback Bridge
std::mutex callback_mutex;

void VendorCallbackWrapper(void* context, ImageData* image) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    // 안전하게 데이터 업데이트
    MyDetector* detector = (MyDetector*)context;
    detector->OnImageReceived(image);
}
```

### 메모리 관리

**제약**: ImageFrame 데이터는 std::shared_ptr로 관리되어야 합니다.

**이유**:
- 16-bit 대용량 이미지 (4K, 8K 등)
- 여러 리스너가 동시에 접근
- 명확한 소유권 이전 메커니즘 필요

**구현 패턴**:
```cpp
std::shared_ptr<ImageFrame> image = std::make_shared<ImageFrame>(width, height);
// Listener들에게 전달 - 포인터만 복사
listener1->OnImageReady(image);
listener2->OnImageReady(image);
// image가 범위를 벗어나도 안전 - 모든 Listener가 완료될 때까지 유효
```

### 에러 코드 매핑

**제약**: 벤더 고유 에러 코드는 UXDI::ErrorCode로 변환되어야 합니다.

**이유**:
- 애플리케이션은 UXDI 에러 코드만 사용
- 벤더별 에러 처리 로직 불필요
- 일관된 예외 처리

**매핑 예시**:
```cpp
enum class ErrorCode {
    Success = 0,
    HardwareNotFound = 100,
    CableDisconnected = 101,
    TemperatureError = 102,
    CalibrationRequired = 103,
    // ...
};

// Varex SDK: 0x8001 → UXDI::ErrorCode::HardwareNotFound
// Vieworks SDK: DEVICE_ERROR → UXDI::ErrorCode::HardwareNotFound
// Samsung SDK: SAM_ERR_DEVICE → UXDI::ErrorCode::HardwareNotFound
// ABYZ SDK: ABYZ_NO_DEVICE → UXDI::ErrorCode::HardwareNotFound
```

---

## 6. 테스트 전략

### 단위 테스트

**테스트 범위**:
- 인터페이스 계약 검증 (Pure Virtual Methods)
- 팩토리 로직 (어댑터 로딩, 인스턴스 생성)
- 상태 전이 (Idle → Preparing → Ready → Exposing)
- 에러 코드 매핑

**예시**:
```cpp
TEST(DetectorInterface, PureVirtualMethodsExist) {
    // IDetector 인터페이스가 모든 필수 메서드 포함 확인
}

TEST(PluginFactory, LoadsAdapterDLL) {
    IDetector* detector = DetectorManager::CreateDetector("Varex");
    EXPECT_NE(detector, nullptr);
}

TEST(ErrorCodeMapping, VarexErrorsTouXDIErrors) {
    int varex_err = 0x8001;
    auto uxdi_err = VarexAdapter::MapErrorCode(varex_err);
    EXPECT_EQ(uxdi_err, ErrorCode::HardwareNotFound);
}
```

### 통합 테스트

**테스트 범위**:
- 어댑터 생명주기 (생성, 초기화, 정리, 소멸)
- Zero-Copy 이미지 파이프라인
- 메모리 누수 검사
- 재연결 스트레스 테스트
- 동시성 테스트 (여러 리스너, 여러 이미지)

**예시**:
```cpp
TEST(VarexAdapterLifecycle, CreateAndDestroy) {
    auto detector = DetectorManager::CreateDetector("Varex");
    EXPECT_TRUE(detector->Initialize());
    EXPECT_EQ(detector->GetState(), DetectorState::Ready);
    EXPECT_TRUE(detector->Shutdown());
    delete detector;
}

TEST(ZeroCopyPipeline, ImageNotCopied) {
    MockListener listener;
    detector->AttachListener(&listener);

    // 이미지 수신 - shared_ptr만 복사됨
    detector->Expose();

    // Listener 확인
    EXPECT_CALL(listener, OnImageReady).Times(1);
}

TEST(MemoryLeakTest, MultipleExposeSequences) {
    for (int i = 0; i < 1000; ++i) {
        detector->Expose();
        detector->WaitForCompletion();
    }
    // Valgrind/Dr. Memory로 메모리 누수 확인
}
```

### Dummy Adapter (시뮬레이터)

**목적**: 실제 하드웨어 없이 개발 및 테스트 가능

**기능**:
- 가상 이미지 생성 (테스트 패턴)
- 노출 시간 시뮬레이션
- 상태 전이 시뮬레이션
- 에러 주입 기능

**사용 예**:
```cpp
// 실제 어댑터 대신 Dummy 사용
IDetector* detector = DetectorManager::CreateDetector("Dummy");
detector->Configure(config);
detector->Expose();  // 가상 노출 지연
// 리스너가 가상 이미지 수신
```

### 커버리지 목표

**목표**: 85% 이상 코드 커버리지

**측정**:
```bash
cmake -DCMAKE_CXX_FLAGS=--coverage ..
make
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

---

## 7. 벤더 SDK 통합 패턴

### 정적 콜백 브릿지 (Static Callback Bridge)

**문제**: C API 콜백 → C++ 인스턴스 메서드 호출

**해결책**: 정적 함수 + context 포인터 사용

**구현**:
```cpp
// Varex SDK (C API)
typedef void (*OnImageCallback)(void* context, VarexImageData* image);

// UXDI Adapter (C++)
class VarexDetector : public IDetector {
private:
    static void OnImageCallbackBridge(void* context, VarexImageData* image) {
        VarexDetector* self = (VarexDetector*)context;
        self->OnImageReceived(image);
    }

    void OnImageReceived(VarexImageData* image) {
        // C++ 구현
    }
};
```

### 설정 매핑 (Config Mapping)

**문제**: 벤더별 서로 다른 설정 형식 (XML, INI, 바이너리)

**해결책**: 벤더별 설정을 UXDI DetectorConfig로 변환

**구현**:
```cpp
class VarexAdapter {
    bool Configure(const DetectorConfig& uxdi_config) {
        VarexConfig varex_cfg;
        varex_cfg.width = uxdi_config.width;
        varex_cfg.height = uxdi_config.height;
        varex_cfg.bit_depth = uxdi_config.bit_depth;
        // ... 추가 매핑
        return sdk_->SetConfiguration(varex_cfg);
    }
};
```

### 상태 기계 (State Machine)

**문제**: 벤더별 서로 다른 상태 정의

**해결책**: UXDI DetectorState로 추상화

**상태 매핑**:
```
Varex SDK States      → UXDI States
├── IDLE              → Idle
├── INITIALIZING      → Initializing
├── READY             → Ready
├── EXPOSING          → Exposing
└── ERROR             → Error

Vieworks SDK States   → UXDI States
├── STATE_STANDBY     → Idle
├── STATE_ACTIVE      → Ready
├── STATE_CAPTURING   → Exposing
└── STATE_FAULT       → Error
```

### 트리거 모드 추상화 (Trigger Mode)

**문제**: 벤더별 트리거 메커니즘 차이

**해결책**: 표준 트리거 모드로 통합

**지원 모드**:
```cpp
enum class TriggerMode {
    Software,      // 소프트웨어 트리거 (Expose() 호출)
    AED,           // 자동 노출 (Automatic Exposure Detection)
    ExternalSync   // 외부 동기 신호
};

bool SetTriggerMode(TriggerMode mode) {
    // Varex SDK의 native mode로 변환
    // Vieworks SDK의 native mode로 변환
}
```

---

## 8. 개발 체크리스트

### Phase 1: 코어 프레임워크 (1-2주)

- [ ] IDetector 인터페이스 설계 완료
- [ ] DetectorConfig, ImageFrame 데이터 구조 정의
- [ ] DetectorManager, PluginFactory 구현
- [ ] CallbackBridge 기본 구현
- [ ] CMake 빌드 시스템 구성
- [ ] 단위 테스트 프레임워크 설정

### Phase 2: 파일럿 구현 (3-5주)

- [ ] Dummy Adapter 완성
- [ ] Varex SDK 분석 및 래핑 시작
- [ ] Varex 콜백 브릿지 구현
- [ ] Varex 에러 코드 매핑
- [ ] Vieworks SDK 분석 시작
- [ ] ABYZ SDK 분석 및 래핑 시작
- [ ] Emulator(emul) 가상 디텍터 모듈 구현
- [ ] 기본 통합 테스트 작성

### Phase 3: 최적화 및 강화 (6-7주)

- [ ] Zero-Copy 메모리 최적화
- [ ] 스레드 안전성 검증
- [ ] 예외 처리 강화 (케이블 분리 감지)
- [ ] 재연결 스트레스 테스트
- [ ] 메모리 누수 검사
- [ ] 성능 프로파일링

---

## 참고 자료

### 공식 문서

- C++20 표준: https://en.cppreference.com/w/cpp
- CMake 문서: https://cmake.org/documentation/
- Google Test: https://github.com/google/googletest
- Windows DLL: https://docs.microsoft.com/en-us/troubleshoot/windows-client/setup-upgrade/windows-dll

### 벤더 SDK

각 제조사별 SDK 문서는 라이선스 계약에 따라 별도 제공됩니다.

- Varex XRpad SDK
- Vieworks VDT SDK
- Samsung S-Detect SDK
- ABYZ SDK
- 기타 제조사 SDK

Emulator(emul) 모듈은 외부 SDK 없이 독립적으로 빌드/실행 가능합니다.

### 개발 도구

- Visual Studio 2022: https://visualstudio.microsoft.com/
- clangd: https://clangd.llvm.org/
- CMake Tools (VS Code): https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools

---

**마지막 업데이트**: 2026-02-16
**상태**: 개발 준비 완료
