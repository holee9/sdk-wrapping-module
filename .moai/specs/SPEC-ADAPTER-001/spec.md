# SPEC-ADAPTER-001: Virtual Adapters (Dummy + Emulator)

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-ADAPTER-001 |
| **제목** | Virtual Adapters (Dummy + Emulator) |
| **Phase** | Phase 1-2 (W2-W4.5) |
| **우선순위** | High |
| **상태** | Planned |
| **생성일** | 2026-02-16 |
| **의존성** | SPEC-CORE-001 |
| **개발 모드** | Hybrid (TDD for new code) |

---

## 1. 개요

하드웨어 의존성 없이 동작하는 두 개의 가상 어댑터를 구현합니다.

**DummyAdapter**: 기본 테스트용 어댑터로, IDetector 인터페이스의 모든 메서드를 최소한으로 구현합니다. .raw 파일에서 이미지를 로드하거나, 파일이 없을 경우 테스트 패턴(gradient/checkerboard)을 생성합니다.

**EmulAdapter**: 프로토콜 정확도 높은 에뮬레이터로, ScenarioEngine을 통해 정상/오류/지연 시나리오를 시뮬레이션합니다. CI/CD 파이프라인, 자동화 테스트, 벤더 SDK 없는 환경에서의 통합 테스트에 활용됩니다.

두 어댑터 모두 uxdi_core + C++ 표준 라이브러리만으로 빌드 가능하며, 벤더 SDK가 필요하지 않습니다.

---

## 2. 환경 (Environment)

- **언어**: C++20 표준
- **빌드 시스템**: CMake 3.20+
- **플랫폼**: Windows (MSVC 2022)
- **의존성**: uxdi_core 정적 라이브러리 (SPEC-CORE-001)
- **외부 SDK**: 불필요 (독립 빌드 가능)
- **출력물**: uxdi_dummy.dll, uxdi_emul.dll

---

## 3. 가정 (Assumptions)

- A-DADP-001: SPEC-CORE-001의 IDetector 인터페이스와 Types.h가 구현 완료되어 사용 가능하다.
- A-DADP-002: DummyAdapter는 단순 테스트용이므로, 실제 디텍터의 타이밍/프로토콜을 정확히 재현할 필요가 없다.
- A-DADP-003: EmulAdapter는 실제 디텍터의 프로토콜(상태 전이, 타이밍, 오류 패턴)을 가능한 한 정확히 에뮬레이션해야 한다.
- A-DADP-004: .raw 파일 형식은 헤더 없는 순수 바이너리 데이터(width * height * depth/8 바이트)이다.
- A-DADP-005: ScenarioEngine의 시나리오는 코드 내 하드코딩된 built-in 시나리오를 사용한다 (향후 외부 파일 로딩 확장 가능).

---

## 4. 요구사항 (Requirements) - EARS 형식

### R-DADP-001: DummyAdapter IDetector 구현

시스템은 **항상** DummyAdapter가 IDetector의 모든 순수 가상 메서드를 구현해야 하며, 하드웨어나 벤더 SDK에 대한 의존성이 없어야 한다.

### R-DADP-002: DummyAdapter 이미지 로딩

**WHEN** DummyAdapter의 Initialize()가 호출되면 **THEN** DetectorConfig의 configPath에 지정된 .raw 파일을 로드하여 이미지 데이터로 사용해야 한다.

### R-DADP-003: DummyAdapter SoftwareTrigger 이미지 전달

**WHEN** DummyAdapter의 SoftwareTrigger()가 호출되면 **THEN** DetectorConfig의 exposureTimeMs만큼 대기(sleep) 후 로드된 이미지를 ImageFrame으로 변환하여 IDetectorListener::OnImageReceived 콜백을 통해 전달해야 한다.

### R-DADP-004: DummyAdapter 테스트 패턴 생성

**IF** configPath가 비어있거나 .raw 파일이 존재하지 않으면 **THEN** DummyAdapter는 테스트 패턴(gradient 또는 checkerboard)을 프로그래밍적으로 생성하여 이미지 데이터로 사용해야 한다.

### R-DADP-005: EmulAdapter IDetector 구현

시스템은 **항상** EmulAdapter가 IDetector의 모든 순수 가상 메서드를 구현해야 하며, 실제 디텍터의 프로토콜 동작을 현실적으로 에뮬레이션해야 한다.

### R-DADP-006: EmulAdapter ScenarioEngine 연동

**WHEN** EmulAdapter가 Initialize되면 **THEN** ScenarioEngine에서 시나리오를 로드하여 각 상태 전이 시 시나리오에 정의된 동작을 수행해야 한다. 시나리오 종류: normal, error, delay.

### R-DADP-007: EmulAdapter 오류 시나리오

**IF** EmulAdapter의 현재 시나리오가 error 타입이면 **THEN** 트리거 시점에 시나리오에 정의된 오류를 주입해야 한다:
- 케이블 분리 (CableDisconnected)
- 온도 경고 (TemperatureError)
- SDK 크래시 (SDKError)

### R-DADP-008: EmulAdapter 지연 시나리오

**IF** EmulAdapter의 현재 시나리오가 delay 타입이면 **THEN** 상태 전이 시점에 시나리오에 정의된 지연(밀리초)을 삽입해야 한다.

### R-DADP-009: ScenarioEngine 시나리오 타입

시스템은 **항상** ScenarioEngine이 최소 3개의 시나리오 타입을 지원해야 한다:
- **normal**: 정상 촬영 흐름 (Connect -> Idle -> Prep -> ReadyToFire -> Acquiring -> Processing -> Idle)
- **error**: 특정 시점에서 오류 발생 (트리거 실패, 케이블 분리 등)
- **delay**: 특정 상태 전이에 인위적 지연 삽입

### R-DADP-010: 벤더 SDK 독립 빌드

시스템은 두 어댑터(DummyAdapter, EmulAdapter)가 uxdi_core와 C++ 표준 라이브러리만으로 빌드 가능하도록 **항상** 벤더 SDK에 대한 의존성이 없어야 한다.

### R-DADP-011: EmulAdapter 순차 프레임 생성

**WHEN** EmulAdapter의 normal 시나리오에서 SoftwareTrigger가 반복 호출되면 **THEN** 생성되는 ImageFrame의 frameId가 순차적으로 증가해야 한다.

### R-DADP-012: DLL Export 함수

시스템은 두 어댑터 DLL이 **항상** 다음 함수를 `extern "C" __declspec(dllexport)`로 export해야 한다:
- `CreateDetector()` - IDetector 인스턴스 생성
- `DestroyDetector(IDetector*)` - IDetector 인스턴스 소멸

---

## 5. 아키텍처

### DummyDetector (단순 어댑터)

```
DummyDetector
├── Initialize() -> .raw 파일 로드 또는 패턴 생성
├── Connect() -> 즉시 Idle 전이
├── Prepare() -> 즉시 ReadyToFire 전이
├── SoftwareTrigger() -> exposureTimeMs 대기 후 이미지 전달
├── Cancel() -> Idle 복귀
└── Disconnect() -> Disconnected 전이
```

### EmulDetector + ScenarioEngine (시나리오 기반 에뮬레이터)

```
EmulDetector
├── Initialize() -> ScenarioEngine 시나리오 로드
├── Connect() -> 시나리오별 지연/오류 적용
├── Prepare() -> 시나리오별 동작 실행
├── SoftwareTrigger() -> 시나리오별 이미지 생성/오류 주입
└── ScenarioEngine
    ├── NormalScenario -> 순차 프레임 생성
    ├── CableDisconnectScenario -> 트리거 시 CableDisconnected 오류
    ├── TemperatureWarningScenario -> 온도 경고 발생
    ├── SDKCrashScenario -> SDK 오류 발생
    ├── SlowInitScenario -> Initialize 지연
    └── SlowPrepScenario -> Prepare 지연
```

---

## 6. ScenarioEngine 설계

### ScenarioAction 구조체

```
ScenarioAction {
    DetectorState targetState;      // 이 액션이 적용될 대상 상태
    uint32_t delayMs;               // 상태 전이 전 지연 시간 (0 = 즉시)
    bool generateImage;             // 이미지 생성 여부
    bool triggerError;              // 오류 주입 여부
    int errorCode;                  // 주입할 ErrorCode (triggerError=true일 때)
    std::string errorMessage;       // 오류 메시지
    double temperature;             // 온도 업데이트 값 (-1 = 업데이트 안함)
}
```

### Built-in 시나리오

| 시나리오 이름 | 타입 | 설명 |
|-------------|------|------|
| `normal` | normal | 정상 촬영 흐름, 순차 프레임 생성 |
| `cable_disconnect` | error | Acquiring 상태에서 CableDisconnected(101) 오류 발생 |
| `temperature_warning` | error | ReadyToFire 상태에서 OnTemperatureUpdate(85.0) 호출 |
| `sdk_crash` | error | SoftwareTrigger 시 SDKError(200) 발생 |
| `slow_init` | delay | Initialize에 3000ms 지연 |
| `slow_prep` | delay | Prepare에 2000ms 지연 |

---

## 7. 파일 분해 (File Decomposition)

### DummyAdapter

| 파일 | 예상 라인 수 | 설명 |
|------|------------|------|
| `src/adapters/dummy/DummyDetector.h` | ~60L | DummyDetector 클래스 선언 |
| `src/adapters/dummy/DummyDetector.cpp` | ~170L | DummyDetector 구현 (.raw 로딩, 패턴 생성) |
| `src/adapters/dummy/DummyAdapter.cpp` | ~25L | DLL Export 함수 (CreateDetector/DestroyDetector) |
| `src/adapters/dummy/CMakeLists.txt` | ~15L | uxdi_dummy DLL 빌드 설정 |

### EmulAdapter

| 파일 | 예상 라인 수 | 설명 |
|------|------------|------|
| `src/adapters/emul/EmulDetector.h` | ~75L | EmulDetector 클래스 선언 |
| `src/adapters/emul/EmulDetector.cpp` | ~230L | EmulDetector 구현 (시나리오 기반 동작) |
| `src/adapters/emul/ScenarioEngine.h` | ~70L | ScenarioEngine 클래스 + ScenarioAction 구조체 |
| `src/adapters/emul/ScenarioEngine.cpp` | ~200L | ScenarioEngine 구현 (built-in 시나리오) |
| `src/adapters/emul/EmulAdapter.cpp` | ~25L | DLL Export 함수 |
| `src/adapters/emul/CMakeLists.txt` | ~15L | uxdi_emul DLL 빌드 설정 |

**총 예상**: 10개 파일, ~885 라인 프로덕션 코드

---

## 8. 의존성

| 의존성 | 유형 | 설명 |
|--------|------|------|
| SPEC-CORE-001 | SPEC 의존성 | IDetector, IDetectorListener, Types.h, Factory.h |
| uxdi_core | 빌드 의존성 | Core 정적 라이브러리에 링크 |
| C++ stdlib | 런타임 | `<thread>`, `<chrono>`, `<fstream>`, `<memory>`, `<vector>` |
| 벤더 SDK | **없음** | 두 어댑터 모두 벤더 SDK 불필요 |

---

## 9. 기술적 위험 요소

| 위험 ID | 설명 | 영향도 | 완화 방안 |
|---------|------|--------|----------|
| TR-DADP-001 | Windows 타이머 정확도 - `std::this_thread::sleep_for`의 해상도가 ~15ms로 낮아 짧은 exposureTimeMs 시뮬레이션이 부정확 | Medium | 높은 해상도 타이머(QueryPerformanceCounter) 사용 고려, 또는 15ms 이하 지연은 busy-wait 적용 |
| TR-DADP-002 | ScenarioEngine 복잡성 - 시나리오 수가 증가하면 유지보수 부담 증가 | Low | Strategy Pattern으로 각 시나리오를 독립 클래스로 분리, 향후 외부 JSON/YAML 시나리오 파일 로딩 지원 |
| TR-DADP-003 | 스레드 안전성 - SoftwareTrigger가 별도 스레드에서 이미지 전달 시 리스너 콜백의 스레드 안전성 | Medium | 콜백 호출 시 mutex 보호, 리스너 등록/해제 시 동기화 |

---

## 추적성 태그 (Traceability)

- **프로젝트 문서**: `.moai/project/product.md` Phase 1 (W1-2) DummyDetector, Phase 2 (W3-5) Emulator
- **구조 문서**: `.moai/project/structure.md` Section 3.3 (어댑터 구현)
- **기술 문서**: `.moai/project/tech.md` Section 4 (어댑터 DLL 빌드), Section 6 (Dummy Adapter)
- **선행 SPEC**: SPEC-CORE-001 (Core Framework)
