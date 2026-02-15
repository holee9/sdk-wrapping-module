# SPEC-ADAPTER-002: Vendor SDK Adapters (Varex, Vieworks, ABYZ)

**SPEC ID:** SPEC-ADAPTER-002
**제목:** Vendor SDK Adapters (Varex, Vieworks, ABYZ)
**Phase:** 2 (W3-5)
**우선순위:** High
**상태:** Planned
**생성일:** 2026-02-16
**의존성:** SPEC-CORE-001, SPEC-ADAPTER-001

---

## 1. 개요

세 개의 벤더별 어댑터(Varex, Vieworks, ABYZ)를 구현하여 각 벤더의 네이티브 SDK를 IDetector 인터페이스로 래핑합니다. 각 어댑터는 독립적인 DLL로 컴파일되며, 벤더 SDK는 해당 DLL 내부에서만 링크됩니다.

SPEC-ADAPTER-001에서 구현된 EmulAdapter를 참조 구현(Reference Implementation)으로 활용하며, 동일한 패턴(Static Callback Bridge, Config Mapping, State Mapping, Error Code Mapping)을 각 벤더 SDK의 특성에 맞게 적용합니다.

---

## 2. EARS 요구사항

### R-VADP-001: IDetector 인터페이스 완전 구현

각 어댑터(Varex, Vieworks, ABYZ)는 **항상** IDetector의 모든 메서드(Connect, Disconnect, GetState, Prepare, Fire, Acquire, GetFrame, AttachListener, DetachListener)를 벤더 SDK를 래핑하여 구현해야 한다.

### R-VADP-002: VarexAdapter 초기화 프로세스

**WHEN** VarexAdapter의 Initialize가 호출되면 **THEN** VarexSDK::Init을 호출하고, Static Callback Bridge를 등록하며, UXDI TriggerMode를 Varex 네이티브 트리거 모드로 매핑해야 한다.

### R-VADP-003: Varex 이미지 콜백 처리

**WHEN** Varex SDK로부터 이미지 콜백이 수신되면 **THEN** RAW 데이터를 ImageFrame으로 래핑하되, 버퍼가 volatile인 경우 shared_ptr로 단일 복사를 수행하고, mutex 보호 하에 OnImageReceived를 디스패치해야 한다.

### R-VADP-004: VieworksAdapter 폴링-이벤트 변환

**WHEN** VieworksAdapter의 Initialize가 호출되면 **THEN** 폴링-이벤트 변환을 위한 전용 스레드를 시작하여 Vieworks SDK의 폴링 기반 API를 UXDI의 이벤트 기반 콜백으로 변환해야 한다.

### R-VADP-005: 공유 상태 보호

**IF** 어댑터가 활성 상태(Prep, ReadyToFire, Acquiring, Processing)에 있고 **WHEN** 공유 상태에 대한 접근이 발생하면 **THEN** 모든 어댑터는 std::mutex를 사용하여 공유 상태를 보호해야 한다.

### R-VADP-006: Static Callback Bridge 구현

각 어댑터는 **항상** C 스타일 콜백을 C++ 인스턴스 메서드로 변환하는 Static Callback Bridge를 구현해야 하며, context 포인터를 통해 인스턴스에 접근해야 한다.

### R-VADP-007: SDK 초기화 실패 처리

**IF** 벤더 SDK 초기화가 실패하면 **THEN** 매핑된 UXDI 에러 코드와 함께 OnError를 호출하고, 어댑터는 Disconnected 상태를 유지해야 한다.

### R-VADP-008: 벤더 에러 코드 매핑

모든 벤더의 에러 코드는 **항상** UXDI::ErrorCode로 매핑되어야 하며, 벤더 고유 에러 코드가 콘솔로 노출되지 않아야 한다.

### R-VADP-009: 벤더 상태 매핑

각 벤더의 고유 상태는 **항상** UXDI DetectorState로 매핑되어야 하며, 상태 전이 시 OnStateChanged 콜백이 호출되어야 한다.

### R-VADP-010: 설정 매핑

UXDI DetectorConfig는 **항상** 각 벤더의 네이티브 설정 형식(XML, INI, API 호출)으로 변환되어야 한다.

### R-VADP-011: Zero-Copy 전략

**가능하면** shared_ptr 래핑을 통한 Zero-Copy를 제공하고, SDK 버퍼가 volatile인 경우에만 단일 복사(single-copy)를 수행해야 한다.

### R-VADP-012: 독립 DLL 컴파일 및 팩토리 함수

각 어댑터는 **항상** 독립적인 DLL로 컴파일되어야 하며, `extern "C"` 선언된 `CreateDetectorInstance()`와 `DestroyDetectorInstance()` 팩토리 함수를 export해야 한다.

---

## 3. 아키텍처

### 3.1 공통 패턴

모든 벤더 어댑터는 다음 4가지 공통 패턴을 구현합니다:

| 패턴 | 설명 |
|------|------|
| **Static Callback Bridge** | C 스타일 SDK 콜백을 C++ 인스턴스 메서드로 변환 (context 포인터 사용) |
| **Config Mapping** | UXDI DetectorConfig를 벤더 네이티브 설정으로 변환 |
| **State Mapping** | 벤더 고유 상태를 UXDI DetectorState로 매핑 |
| **Error Code Mapping** | 벤더 에러 코드를 UXDI::ErrorCode로 변환 |

### 3.2 VarexAdapter

- **SDK 유형:** 콜백 기반(Callback-based)
- **콜백 패턴:** Context 포인터를 통한 Static Callback Bridge
- **메모리 전략:** 필수 복사(Mandatory Copy) - Varex SDK 버퍼는 volatile이며 콜백 반환 후 재사용됨
- **특이사항:** VarexSDK::Init 호출 시 콜백 함수와 context 포인터를 동시에 등록해야 함

### 3.3 VieworksAdapter

- **SDK 유형:** 폴링 기반(Polling-based)
- **이벤트 변환:** 전용 폴링 스레드가 SDK를 주기적으로 조회하여 UXDI 이벤트로 변환
- **메모리 전략:** SDK 문서 확인 후 결정 (Zero-Copy 가능 여부 평가 필요)
- **특이사항:** 폴링 주기 최적화 필요, 스레드 안전한 종료 보장

### 3.4 ABYZAdapter

- **SDK 유형:** TBD (SDK API 스타일 확인 후 결정)
- **메모리 전략:** 기본 복사(Default Copy) - SDK 확인 후 최적화
- **특이사항:** 스켈레톤 코드로 시작, TODO 마커로 미구현 부분 표시

---

## 4. 상태 매핑 테이블

### 4.1 Varex State Mapping

| Varex SDK State | UXDI DetectorState |
|-----------------|-------------------|
| VAREX_IDLE | Idle |
| VAREX_PREPARING | Prep |
| VAREX_READY | ReadyToFire |
| VAREX_EXPOSING | Acquiring |
| VAREX_PROCESSING | Processing |
| VAREX_ERROR | Error |

### 4.2 Vieworks State Mapping

| Vieworks SDK State | UXDI DetectorState |
|-------------------|-------------------|
| VW_STATE_STANDBY | Idle |
| VW_STATE_ACTIVE | ReadyToFire |
| VW_STATE_CAPTURING | Acquiring |
| VW_STATE_FAULT | Error |

### 4.3 ABYZ State Mapping

| ABYZ SDK State | UXDI DetectorState |
|---------------|-------------------|
| TBD | TBD (SDK 문서 확인 후 매핑) |

---

## 5. Zero-Copy 전략

| 어댑터 | 전략 | 근거 |
|--------|------|------|
| **VarexAdapter** | 필수 복사 (Mandatory Copy) | Varex SDK 버퍼가 volatile이며 콜백 반환 후 재사용됨 |
| **VieworksAdapter** | SDK 의존 (Depends on SDK) | SDK 문서 확인 후 Zero-Copy 가능 여부 결정 |
| **ABYZAdapter** | 기본 복사 (Default Copy) | 안전한 기본값, SDK 확인 후 최적화 가능 |

---

## 6. 파일 분해

### 신규 생성 파일

| 파일 경로 | 예상 라인 수 | 설명 |
|-----------|-------------|------|
| `src/adapters/varex/VarexDetector.h` | ~80L | Varex 어댑터 클래스 선언 |
| `src/adapters/varex/VarexDetector.cpp` | ~260L | Varex 어댑터 구현 |
| `src/adapters/varex/VarexAdapter.cpp` | ~25L | DLL Export 팩토리 함수 |
| `src/adapters/varex/CMakeLists.txt` | ~20L | Varex 빌드 설정 |
| `src/adapters/vieworks/VieworksDetector.h` | ~90L | Vieworks 어댑터 클래스 선언 |
| `src/adapters/vieworks/VieworksDetector.cpp` | ~295L | Vieworks 어댑터 구현 (폴링 스레드 포함) |
| `src/adapters/vieworks/VieworksAdapter.cpp` | ~25L | DLL Export 팩토리 함수 |
| `src/adapters/vieworks/CMakeLists.txt` | ~20L | Vieworks 빌드 설정 |
| `src/adapters/abyz/ABYZDetector.h` | ~75L | ABYZ 어댑터 클래스 선언 |
| `src/adapters/abyz/ABYZDetector.cpp` | ~230L | ABYZ 어댑터 구현 (스켈레톤) |
| `src/adapters/abyz/ABYZAdapter.cpp` | ~25L | DLL Export 팩토리 함수 |
| `src/adapters/abyz/CMakeLists.txt` | ~20L | ABYZ 빌드 설정 |
| `cmake/FindVendorSDK.cmake` | ~80L | 벤더 SDK 자동 검색 모듈 |

**총계:** 13개 신규 파일, 약 1,200줄 프로덕션 코드

---

## 7. 의존성

### SPEC 의존성 트리

```
SPEC-CORE-001 (의존성 없음)
    |
    +-> SPEC-ADAPTER-001 (CORE-001에 의존)
    |       |
    |       +-> SPEC-ADAPTER-002 (CORE-001, ADAPTER-001에 의존)  <-- 현재 SPEC
    |
    +-> SPEC-INTEG-001 (CORE-001, ADAPTER-001, ADAPTER-002에 의존)
```

### 기술적 의존성

- **SPEC-CORE-001:** IDetector, IDetectorListener, DetectorState, ImageFrame, DetectorConfig, ErrorCode, DetectorFactory 인터페이스 및 타입 정의
- **SPEC-ADAPTER-001:** EmulAdapter 참조 구현 - 공통 패턴(Callback Bridge, Config Mapping, State Mapping, Error Code Mapping)의 기준 구현

---

## 8. 기술적 위험 요소

| 위험 | 영향도 | 발생 가능성 | 대응 방안 |
|------|--------|------------|----------|
| **SDK API 가변성:** 각 벤더 SDK의 API 스타일이 크게 다를 수 있음 | High | High | EmulAdapter의 공통 패턴을 기반으로 하되, 벤더별 특화 로직을 유연하게 수용 |
| **SDK 접근성:** NDA/라이선스 제약으로 SDK를 적시에 확보하지 못할 수 있음 | High | Medium | Mock SDK 레이어를 활용하여 SDK 미확보 시에도 개발 진행 가능하도록 설계 |
| **Zero-Copy 실현 가능성:** 일부 SDK의 버퍼 관리 방식이 Zero-Copy를 허용하지 않을 수 있음 | Medium | Medium | Mandatory Copy를 기본값으로 하되, SDK별 Zero-Copy 가능 여부를 검증 |
| **SDK 버전 비호환:** 벤더 SDK 업데이트 시 기존 어댑터와 호환되지 않을 수 있음 | Medium | Low | 벤더별 버전 매핑 레이어를 두어 SDK 버전 차이를 흡수 |

---

## 9. 추적성 태그

- [SPEC-ADAPTER-002] -> [SPEC-CORE-001]: IDetector 인터페이스 구현
- [SPEC-ADAPTER-002] -> [SPEC-ADAPTER-001]: EmulAdapter 패턴 참조
- [SPEC-ADAPTER-002] -> [SPEC-INTEG-001]: 통합 테스트 및 성능 검증 대상
