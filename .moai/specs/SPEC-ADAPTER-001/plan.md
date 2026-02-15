# SPEC-ADAPTER-001: 구현 계획

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-ADAPTER-001 |
| **제목** | Virtual Adapters (Dummy + Emulator) - 구현 계획 |
| **개발 모드** | Hybrid (TDD - 모든 코드가 신규) |
| **예상 파일 수** | 10개 프로덕션 파일 |
| **예상 코드 라인** | ~885L |

---

## 1. 구현 단계 (우선순위 기반)

### Primary Goal: DummyAdapter 구현 (W2)

**우선순위**: Critical - Core Framework 검증의 첫 번째 통합 대상

**대상 파일:**
1. `src/adapters/dummy/DummyDetector.h` - 클래스 선언
2. `src/adapters/dummy/DummyDetector.cpp` - 핵심 구현
3. `src/adapters/dummy/DummyAdapter.cpp` - DLL Export 함수
4. `src/adapters/dummy/CMakeLists.txt` - 빌드 설정

**TDD 접근:**
- RED: IDetector 메서드 호출 테스트, .raw 파일 로딩 테스트, 패턴 생성 테스트
- GREEN: 각 메서드 최소 구현
- REFACTOR: 이미지 생성 로직 분리, 에러 처리 강화

**구현 순서:**
1. DummyDetector 클래스 선언 (IDetector 상속)
2. 상태 머신 구현 (Disconnected -> Idle -> Prep -> ReadyToFire -> Acquiring -> Idle)
3. .raw 파일 로딩 구현
4. 테스트 패턴 생성 (gradient, checkerboard)
5. SoftwareTrigger 이미지 전달 (sleep + 콜백)
6. DLL Export 함수 구현
7. CMakeLists.txt 구성

**완료 기준:**
- Factory를 통한 DummyAdapter DLL 로딩 성공
- 모든 IDetector 메서드 호출 가능
- .raw 파일 로딩 및 테스트 패턴 생성 검증
- 상태 전이 규칙 준수

---

### Secondary Goal: ScenarioEngine 구현 (W3-W4)

**우선순위**: High - EmulAdapter의 핵심 엔진

**대상 파일:**
1. `src/adapters/emul/ScenarioEngine.h` - ScenarioAction 구조체 + ScenarioEngine 클래스
2. `src/adapters/emul/ScenarioEngine.cpp` - Built-in 시나리오 구현

**TDD 접근:**
- RED: 각 시나리오 타입별 동작 테스트
- GREEN: ScenarioAction 구조체 및 built-in 시나리오 구현
- REFACTOR: 시나리오 확장성 개선

**테스트 시나리오:**
- normal 시나리오: 모든 상태 전이가 정상 수행
- cable_disconnect 시나리오: Acquiring 상태에서 CableDisconnected 오류 발생
- temperature_warning 시나리오: OnTemperatureUpdate 콜백 호출
- sdk_crash 시나리오: SDKError 발생
- slow_init 시나리오: Initialize에 지연 삽입
- slow_prep 시나리오: Prepare에 지연 삽입

**완료 기준:**
- 6개 built-in 시나리오 모두 구현 및 테스트 통과
- 시나리오 이름으로 로딩 가능
- ScenarioAction 시퀀스가 올바른 순서로 실행

---

### Tertiary Goal: EmulAdapter 구현 (W4-W4.5)

**우선순위**: High - 프로토콜 정확도 높은 에뮬레이터

**대상 파일:**
1. `src/adapters/emul/EmulDetector.h` - 클래스 선언
2. `src/adapters/emul/EmulDetector.cpp` - ScenarioEngine 기반 구현
3. `src/adapters/emul/EmulAdapter.cpp` - DLL Export 함수
4. `src/adapters/emul/CMakeLists.txt` - 빌드 설정

**TDD 접근:**
- RED: 각 시나리오별 통합 동작 테스트
- GREEN: ScenarioEngine과 통합된 IDetector 구현
- REFACTOR: 시나리오 전환, 에러 복구 로직 개선

**구현 순서:**
1. EmulDetector 클래스 선언 (IDetector 상속)
2. ScenarioEngine 통합 (시나리오 로딩)
3. 상태 전이 시 ScenarioAction 적용
4. 오류 시나리오 주입 로직
5. 지연 시나리오 적용 로직
6. 순차 프레임 생성 (frameId 자동 증가)
7. DLL Export 함수 및 CMakeLists.txt

**완료 기준:**
- 모든 시나리오(normal, error, delay) 동작 검증
- Factory를 통한 EmulAdapter DLL 로딩 성공
- 순차 프레임 생성 검증 (frameId 증가)
- 오류/지연 주입 타이밍 정확성 검증

---

### Optional Goal: 통합 테스트

**우선순위**: Medium - Core + Adapter 통합 검증

**대상:**
- Factory로 DummyAdapter 로딩 -> 전체 촬영 사이클 실행
- Factory로 EmulAdapter 로딩 -> 각 시나리오별 통합 동작
- Manager에 두 어댑터 동시 등록 -> 독립 동작 검증

---

## 2. 기술적 접근 방식

### TDD 전략 (신규 프로젝트)

모든 코드가 신규이므로 TDD(RED-GREEN-REFACTOR) 사이클을 적용합니다.

**테스트 우선순위:**
1. 단위 테스트: DummyDetector 메서드, ScenarioEngine 시나리오, EmulDetector 통합 동작
2. DLL 통합 테스트: Factory를 통한 DLL 로딩 및 인스턴스 생성
3. 시나리오 테스트: 각 built-in 시나리오의 정확한 동작 검증

### DummyDetector 설계

- **단순성 우선**: 최소한의 로직으로 IDetector 인터페이스 구현
- **이미지 소스**: .raw 파일 로딩 우선, 파일 없으면 패턴 생성
- **상태 머신**: 즉시 전이 (지연 없음, exposureTimeMs 대기만 SoftwareTrigger에서)
- **스레드**: SoftwareTrigger의 sleep은 호출 스레드에서 수행 (단순 구현)

### EmulDetector + ScenarioEngine 설계

- **시나리오 기반**: 모든 동작이 ScenarioAction 시퀀스에 의해 결정
- **프로토콜 정확도**: 실제 디텍터의 상태 전이 타이밍을 근사
- **오류 주입**: 시나리오에 정의된 시점에서 정확한 ErrorCode와 메시지 전달
- **확장성**: 향후 외부 파일(JSON/YAML)에서 시나리오 로딩 가능하도록 설계

### 코드 스타일

- Google C++ Style Guide 준수
- Doxygen 주석 형식
- 네임스페이스: `uxdi::adapters::dummy`, `uxdi::adapters::emul`

---

## 3. 테스트 계획

### 단위 테스트 파일 구조

```
tests/
├── unit/
│   ├── test_dummy_detector.cpp     - DummyDetector 메서드 테스트
│   ├── test_dummy_image.cpp        - .raw 로딩 + 패턴 생성 테스트
│   ├── test_scenario_engine.cpp    - ScenarioEngine 시나리오 테스트
│   ├── test_emul_detector.cpp      - EmulDetector 통합 동작 테스트
│   └── test_emul_scenarios.cpp     - 각 시나리오별 상세 테스트
├── integration/
│   ├── test_dummy_lifecycle.cpp    - DummyAdapter DLL 로딩 + 전체 사이클
│   └── test_emul_lifecycle.cpp     - EmulAdapter DLL 로딩 + 시나리오 실행
```

### 테스트 커버리지 목표

**DummyAdapter 테스트:**
- 상태 전이: Disconnected -> Idle -> Prep -> ReadyToFire -> Acquiring -> Idle
- .raw 파일 로딩: 유효한 파일, 존재하지 않는 파일, 빈 configPath
- 테스트 패턴: gradient 패턴 픽셀 값 검증, checkerboard 패턴 검증
- SoftwareTrigger: exposureTimeMs 대기 검증, OnImageReceived 콜백 호출 검증
- DLL Export: CreateDetector/DestroyDetector 함수 존재 및 동작 검증

**EmulAdapter 테스트:**
- normal 시나리오: 전체 촬영 사이클, frameId 순차 증가 검증
- cable_disconnect 시나리오: 정확한 타이밍에 CableDisconnected 오류 발생
- temperature_warning 시나리오: OnTemperatureUpdate(85.0) 콜백 검증
- sdk_crash 시나리오: SDKError 발생 검증
- slow_init 시나리오: Initialize 지연 시간 검증
- slow_prep 시나리오: Prepare 지연 시간 검증

---

## 4. 위험 관리

| 위험 | 완화 전략 |
|------|----------|
| Windows 타이머 해상도 (~15ms) | 짧은 지연은 busy-wait, 긴 지연은 sleep_for 조합 |
| ScenarioEngine 복잡성 증가 | Strategy Pattern으로 시나리오별 독립 클래스화 |
| 스레드 안전성 (콜백) | 리스너 콜백에 mutex 보호, 등록/해제 동기화 |

---

## 5. 빌드 설정

### DummyAdapter DLL

```
# src/adapters/dummy/CMakeLists.txt
add_library(uxdi_dummy SHARED
    DummyDetector.cpp
    DummyAdapter.cpp
)
target_link_libraries(uxdi_dummy PRIVATE uxdi_core)
```

### EmulAdapter DLL

```
# src/adapters/emul/CMakeLists.txt
add_library(uxdi_emul SHARED
    EmulDetector.cpp
    ScenarioEngine.cpp
    EmulAdapter.cpp
)
target_link_libraries(uxdi_emul PRIVATE uxdi_core)
```

---

## 6. 후속 단계

- 실제 벤더 어댑터(Varex, Vieworks 등) 구현 시 EmulAdapter를 참조 구현으로 활용
- ScenarioEngine 확장: 외부 JSON/YAML 파일에서 커스텀 시나리오 로딩
- CI/CD 파이프라인에서 EmulAdapter를 활용한 자동화 통합 테스트

---

## 추적성 태그

- **SPEC 참조**: SPEC-ADAPTER-001/spec.md
- **수락 기준**: SPEC-ADAPTER-001/acceptance.md
- **선행 SPEC**: SPEC-CORE-001
- **프로젝트 Phase**: Phase 1 (W2) DummyAdapter, Phase 1-2 (W4-W4.5) EmulAdapter
