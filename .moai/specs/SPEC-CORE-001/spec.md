# SPEC-CORE-001: UXDI Core Framework

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-CORE-001 |
| **제목** | UXDI Core Framework |
| **Phase** | Phase 1 (W1-2) |
| **우선순위** | Critical |
| **상태** | Planned |
| **생성일** | 2026-02-16 |
| **의존성** | 없음 (Foundation SPEC) |
| **개발 모드** | Hybrid (TDD for new code) |

---

## 1. 개요

UXDI(Universal X-ray Detector Interface)의 핵심 프레임워크를 구축합니다. IDetector 추상 기본 클래스(ABC), 공용 타입 정의(Types.h), IDetectorListener 콜백 인터페이스, DetectorFactory(DLL 플러그인 로더), DetectorManager(생명주기 관리자)를 포함합니다.

이 SPEC은 모든 벤더 어댑터의 기반이 되는 Foundation SPEC으로, 외부 의존성 없이 C++20 표준 라이브러리만으로 구현됩니다. ABI 안정성을 보장하기 위해 순수 가상 인터페이스와 가상 소멸자만 사용하며, 멤버 변수를 포함하지 않습니다.

---

## 2. 환경 (Environment)

- **언어**: C++20 표준
- **빌드 시스템**: CMake 3.20+
- **플랫폼**: Windows (MSVC 2022), 향후 Linux 확장 고려
- **테스트 프레임워크**: Google Test (GTest)
- **Core 라이브러리 형태**: 정적 라이브러리 (uxdi_core)
- **어댑터 형태**: DLL 동적 라이브러리 (플러그인)

---

## 3. 가정 (Assumptions)

- A-CORE-001: Windows 환경에서 LoadLibraryA/GetProcAddress를 사용한 DLL 동적 로딩이 가능하다.
- A-CORE-002: 모든 어댑터 DLL은 `CreateDetector`와 `DestroyDetector` extern "C" 함수를 export한다.
- A-CORE-003: IDetector 인터페이스는 순수 가상 함수와 가상 소멸자만 포함하며, 멤버 변수는 절대 포함하지 않는다.
- A-CORE-004: ImageFrame의 이미지 데이터는 `shared_ptr<uint8_t[]>`로 관리되어 Zero-Copy 파이프라인을 지원한다.
- A-CORE-005: 상태 머신은 DetectorState enum으로 통합 관리되며, 모든 어댑터가 동일한 상태 전이 규칙을 따른다.

---

## 4. 요구사항 (Requirements) - EARS 형식

### R-CORE-001: IDetector 순수 가상 인터페이스

시스템은 **항상** IDetector 추상 기본 클래스를 다음과 같이 제공해야 한다:
- 순수 가상 메서드: `Initialize()`, `Connect()`, `Disconnect()`, `Prepare()`, `SoftwareTrigger()`, `Cancel()`, `GetState()`, `SetListener()`, `SetParameter()`
- `virtual ~IDetector() = default;` 가상 소멸자 필수
- 멤버 변수 **절대 금지** (ABI 안정성 보장)

### R-CORE-002: 공용 타입 정의 (Types.h)

시스템은 **항상** 다음 공용 타입을 Types.h에 정의해야 한다:

**ImageFrame 구조체:**
- `uint32_t width` - 이미지 너비 (픽셀)
- `uint32_t height` - 이미지 높이 (픽셀)
- `uint32_t depth` - 비트 깊이
- `uint64_t frameId` - 프레임 식별자
- `uint64_t timestamp` - 타임스탬프 (밀리초)
- `std::shared_ptr<uint8_t[]> data` - 이미지 데이터 (Zero-Copy)
- `size_t dataSize` - 데이터 크기 (바이트)

**DetectorConfig 구조체:**
- `std::string configPath` - 설정 파일 경로
- `TriggerMode triggerMode` - 트리거 모드
- `double exposureTimeMs` - 노출 시간 (밀리초)
- `bool useInternalCorrection` - 내부 보정 사용 여부

**DetectorState enum:**
- `Disconnected`, `Idle`, `Prep`, `ReadyToFire`, `Acquiring`, `Processing`, `Error`

**TriggerMode enum:**
- `Software`, `AED`, `ExternalSync`

**ErrorCode enum:**
- `Success = 0`
- `HardwareNotFound = 100`, `CableDisconnected = 101`, `TemperatureError = 102`, `CalibrationRequired = 103`
- `SDKError = 200`
- `Timeout = 300`
- `Unknown = 999`

### R-CORE-003: IDetectorListener 콜백 인터페이스

시스템은 **항상** IDetectorListener 인터페이스를 다음 순수 가상 콜백 메서드로 제공해야 한다:
- `OnStateChanged(DetectorState newState)` - 상태 변경 알림
- `OnImageReceived(const ImageFrame& frame)` - 이미지 수신 알림
- `OnError(int errorCode, const std::string& message)` - 오류 발생 알림
- `OnTemperatureUpdate(double temperature)` - 온도 업데이트 알림

### R-CORE-004: DetectorFactory DLL 동적 로딩

**WHEN** DetectorFactory::LoadAdapter(dllPath)가 호출되면 **THEN** 시스템은 다음을 수행해야 한다:
1. `LoadLibraryA(dllPath)`로 DLL 로드
2. `GetProcAddress(hLib, "CreateDetector")`로 CreateDetector 함수 포인터 획득
3. `GetProcAddress(hLib, "DestroyDetector")`로 DestroyDetector 함수 포인터 획득
4. CreateDetector()를 호출하여 IDetector 인스턴스 생성
5. DestroyDetector를 custom deleter로 사용하는 `shared_ptr<IDetector>` 반환

### R-CORE-005: Factory 오류 처리

**IF** DLL 로드가 실패하면 **THEN** 시스템은 `std::runtime_error`를 throw해야 한다 (DLL 파일 경로 포함).

**IF** CreateDetector 또는 DestroyDetector 심볼 해석이 실패하면 **THEN** 시스템은 `std::runtime_error`를 throw해야 한다 (실패한 심볼 이름 포함).

### R-CORE-006: 상태 머신 - Disconnected 상태

**IF** 디텍터가 Disconnected 상태이면 **THEN** Connect()와 Initialize()만 허용되어야 한다.
다른 메서드 호출 시 OnError 콜백을 통해 오류를 알려야 한다.

### R-CORE-007: 상태 머신 - Idle 상태

**IF** 디텍터가 Idle 상태이면 **THEN** Prepare(), Disconnect(), SetParameter()가 허용되어야 한다.

### R-CORE-008: 상태 머신 - ReadyToFire 상태

**IF** 디텍터가 ReadyToFire 상태이면 **THEN** 다음이 허용되어야 한다:
- SoftwareTrigger() (TriggerMode가 Software인 경우에만)
- Cancel()

### R-CORE-009: 상태 전이 알림

**WHEN** 디텍터의 상태가 변경되면 **THEN** 등록된 모든 IDetectorListener의 OnStateChanged 콜백이 호출되어야 한다.

### R-CORE-010: SoftwareTrigger 오류 처리

**IF** SoftwareTrigger()가 ReadyToFire 상태가 아닐 때 호출되면 **THEN** 시스템은 OnError(400, "Detector Not Ready") 콜백을 호출해야 한다.

### R-CORE-011: DetectorManager 생명주기 관리

시스템은 **항상** DetectorManager를 통해 다음 생명주기 관리 기능을 제공해야 한다:
- `RegisterDetector(name, detector)` - 디텍터 등록
- `GetDetector(name)` - 이름으로 디텍터 조회
- `UnregisterDetector(name)` - 디텍터 등록 해제
- `ShutdownAll()` - 모든 디텍터 안전 종료

### R-CORE-012: ABI 안정성

시스템은 IDetector와 IDetectorListener 인터페이스에서 다음을 **하지 않아야 한다**:
- 멤버 변수 선언
- 비가상 메서드 정의 (순수 가상 + 가상 소멸자만 허용)
- inline 구현 포함

---

## 5. 아키텍처

### 설계 패턴

- **Pure Virtual ABC**: IDetector, IDetectorListener는 순수 가상 인터페이스
- **Factory Pattern**: DetectorFactory가 DLL에서 IDetector 인스턴스 생성
- **Observer Pattern**: IDetectorListener를 통한 비동기 이벤트 알림
- **Plugin Architecture**: 런타임 DLL 동적 로딩

### Core 라이브러리 구성

```
uxdi_core (정적 라이브러리)
├── include/uxdi/
│   ├── Types.h              - 공용 타입 정의
│   ├── IDetectorListener.h  - 리스너 인터페이스
│   ├── IDetector.h          - 핵심 추상 인터페이스
│   ├── Factory.h            - DetectorFactory 선언
│   └── Export.h             - DLL Export 매크로
├── src/core/
│   ├── DetectorFactory.cpp  - DLL 로더 구현
│   └── DetectorManager.cpp  - 생명주기 관리 구현
```

---

## 6. 파일 분해 (File Decomposition)

| 파일 | 예상 라인 수 | 설명 |
|------|------------|------|
| `include/uxdi/Types.h` | ~80L | 공용 타입, enum, 구조체 정의 |
| `include/uxdi/IDetectorListener.h` | ~35L | 리스너 콜백 인터페이스 |
| `include/uxdi/IDetector.h` | ~55L | 핵심 추상 인터페이스 |
| `include/uxdi/Factory.h` | ~45L | DetectorFactory 클래스 선언 |
| `include/uxdi/Export.h` | ~25L | DLL Export/Import 매크로 |
| `src/core/DetectorFactory.cpp` | ~120L | DLL 로딩 및 인스턴스 생성 |
| `src/core/DetectorManager.cpp` | ~150L | 디텍터 등록/조회/종료 관리 |
| `CMakeLists.txt` (루트) | ~60L | 프로젝트 루트 빌드 설정 |
| `src/CMakeLists.txt` | ~30L | Core 라이브러리 빌드 설정 |

**총 예상**: 9개 파일, ~600 라인 프로덕션 코드

---

## 7. 의존성

- **외부 의존성**: 없음 (C++20 표준 라이브러리만 사용)
- **시스템 의존성**: Windows API (LoadLibraryA, GetProcAddress, FreeLibrary)
- **테스트 의존성**: Google Test (GTest)

이 SPEC은 Foundation SPEC으로, 다른 모든 어댑터 SPEC의 기반이 됩니다.

---

## 8. 기술적 위험 요소

| 위험 ID | 설명 | 영향도 | 완화 방안 |
|---------|------|--------|----------|
| TR-CORE-001 | ABI/vtable 불일치 - 서로 다른 컴파일러 버전으로 빌드된 DLL 간 vtable 레이아웃 불일치 | High | 동일한 MSVC 버전으로 Core와 어댑터 빌드, ABI 테스트 추가 |
| TR-CORE-002 | FreeLibrary 호출 순서 - IDetector 인스턴스가 해제되기 전에 DLL이 언로드되면 crash | High | shared_ptr custom deleter에서 순서 보장, DLL 핸들을 shared_ptr로 관리 |
| TR-CORE-003 | DLL 로딩 스레드 안전성 - 여러 스레드에서 동시에 LoadAdapter 호출 시 경합 | Medium | DetectorFactory에 mutex 보호 추가 |

---

## 추적성 태그 (Traceability)

- **프로젝트 문서**: `.moai/project/product.md` Phase 1 (W1-2)
- **구조 문서**: `.moai/project/structure.md` Section 3.1, 3.2
- **기술 문서**: `.moai/project/tech.md` Section 2, 4
- **후속 SPEC**: SPEC-ADAPTER-001 (이 SPEC에 의존)
