# SPEC-ADAPTER-001: 수락 기준 (Acceptance Criteria)

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-ADAPTER-001 |
| **제목** | Virtual Adapters (Dummy + Emulator) - 수락 기준 |
| **검증 방법** | 단위 테스트 (Google Test) + DLL 통합 테스트 |

---

## 1. 수락 기준 (Acceptance Criteria)

### AC-DADP-01: DummyAdapter IDetector 완전 구현

**Given** DummyAdapter DLL(uxdi_dummy.dll)이 빌드되었을 때
**When** DetectorFactory::LoadAdapter("uxdi_dummy.dll")로 로딩할 때
**Then** 유효한 shared_ptr<IDetector>가 반환되어야 한다
**And** IDetector의 모든 순수 가상 메서드(Initialize, Connect, Disconnect, Prepare, SoftwareTrigger, Cancel, GetState, SetListener, SetParameter)가 호출 가능해야 한다

### AC-DADP-02: DummyAdapter .raw 파일 이미지 로딩

**Given** 유효한 .raw 파일 경로가 DetectorConfig.configPath에 설정되어 있을 때
**When** DummyAdapter의 Initialize()를 호출할 때
**Then** .raw 파일의 바이너리 데이터가 ImageFrame.data에 로드되어야 한다
**And** SoftwareTrigger() 호출 시 해당 이미지가 OnImageReceived 콜백으로 전달되어야 한다

### AC-DADP-03: DummyAdapter 테스트 패턴 생성

**Given** DetectorConfig.configPath가 비어있거나 존재하지 않는 파일 경로일 때
**When** DummyAdapter의 Initialize()를 호출할 때
**Then** gradient 또는 checkerboard 테스트 패턴이 프로그래밍적으로 생성되어야 한다
**And** 생성된 패턴의 크기가 width * height * (depth/8) 바이트와 일치해야 한다

### AC-DADP-04: DummyAdapter 노출 시간 시뮬레이션

**Given** DetectorConfig.exposureTimeMs가 100ms로 설정되어 있을 때
**When** SoftwareTrigger()를 호출할 때
**Then** OnImageReceived 콜백이 호출되기까지 약 100ms(+/- 20ms 허용)가 소요되어야 한다

### AC-DADP-05: EmulAdapter 정상 시나리오 (normal)

**Given** EmulAdapter가 "normal" 시나리오로 초기화되었을 때
**When** 전체 촬영 사이클(Connect -> Prepare -> SoftwareTrigger)을 실행할 때
**Then** 상태 전이가 Disconnected -> Idle -> Prep -> ReadyToFire -> Acquiring -> Processing -> Idle 순서로 발생해야 한다
**And** OnImageReceived 콜백이 호출되어야 한다
**And** 반복 호출 시 ImageFrame.frameId가 순차적으로 증가해야 한다

### AC-DADP-06: EmulAdapter 오류 시나리오 (cable_disconnect)

**Given** EmulAdapter가 "cable_disconnect" 시나리오로 초기화되었을 때
**When** SoftwareTrigger()를 호출하여 Acquiring 상태에 진입할 때
**Then** OnError(101, ...) 콜백이 호출되어야 한다 (ErrorCode::CableDisconnected)
**And** 디텍터 상태가 Error로 전이되어야 한다

### AC-DADP-07: EmulAdapter 지연 시나리오 (slow_init)

**Given** EmulAdapter가 "slow_init" 시나리오로 초기화되었을 때
**When** Initialize()를 호출할 때
**Then** Initialize 완료까지 약 3000ms(+/- 500ms 허용)가 소요되어야 한다

### AC-DADP-08: DLL Export 함수 검증

**Given** uxdi_dummy.dll과 uxdi_emul.dll이 빌드되었을 때
**When** GetProcAddress로 "CreateDetector"와 "DestroyDetector" 심볼을 조회할 때
**Then** 두 함수 모두 nullptr가 아닌 유효한 함수 포인터가 반환되어야 한다
**And** CreateDetector()는 유효한 IDetector 포인터를 반환해야 한다
**And** DestroyDetector(ptr)는 crash 없이 인스턴스를 소멸해야 한다

---

## 2. 엣지 케이스 (Edge Cases)

### EC-DADP-01: 빈 .raw 파일

**Given** 크기가 0인 .raw 파일이 configPath에 지정되었을 때
**When** DummyAdapter의 Initialize()를 호출할 때
**Then** .raw 파일 로딩을 실패로 처리하고 테스트 패턴으로 fallback해야 한다
**And** OnError 콜백을 통해 경고를 전달해야 한다

### EC-DADP-02: SoftwareTrigger 연속 호출

**Given** DummyAdapter가 ReadyToFire 상태일 때
**When** SoftwareTrigger()를 빠르게 연속 2회 호출할 때
**Then** 첫 번째 호출은 정상 처리되어야 한다
**And** 두 번째 호출은 Acquiring 상태이므로 OnError 콜백으로 거부되어야 한다

### EC-DADP-03: 존재하지 않는 시나리오 이름

**Given** EmulAdapter에 존재하지 않는 시나리오 이름("invalid_scenario")이 지정되었을 때
**When** Initialize()를 호출할 때
**Then** 기본 시나리오("normal")로 fallback하거나 OnError를 통해 알려야 한다

### EC-DADP-04: EmulAdapter Disconnect 후 재연결

**Given** EmulAdapter가 정상 촬영 사이클을 완료한 후 Disconnect()한 상태일 때
**When** 다시 Connect()를 호출할 때
**Then** 정상적으로 Idle 상태로 전이되어야 한다
**And** 이전 시나리오가 초기화되어 처음부터 다시 실행 가능해야 한다

### EC-DADP-05: 리스너 미등록 시 이미지 전달

**Given** IDetectorListener가 등록되지 않은 DummyAdapter에서
**When** SoftwareTrigger()를 호출할 때
**Then** crash나 예외 없이 정상적으로 완료되어야 한다
**And** 이미지는 생성되지만 콜백이 호출되지 않아야 한다

---

## 3. 위험 요소 (Risk Factors)

### RF-DADP-01: Windows 타이머 정확도

**위험**: `std::this_thread::sleep_for`의 해상도가 ~15ms로, 짧은 exposureTimeMs(예: 5ms) 시뮬레이션이 부정확할 수 있다
**영향도**: Medium
**검증 방법**: 타이밍 테스트에서 +/- 20ms 허용 범위 적용, 필요시 QueryPerformanceCounter 기반 고해상도 타이머 도입

### RF-DADP-02: ScenarioEngine 상태 관리

**위험**: 복잡한 시나리오에서 상태 머신과 ScenarioAction의 동기화가 깨질 수 있다
**영향도**: Medium
**검증 방법**: 각 시나리오의 전체 상태 전이를 순서대로 검증하는 테스트, 상태 불일치 시 자동 복구 로직

### RF-DADP-03: DLL 빌드 호환성

**위험**: uxdi_core와 어댑터 DLL이 서로 다른 빌드 설정(Debug/Release, CRT 버전)으로 빌드되면 런타임 crash 발생
**영향도**: High
**검증 방법**: CMake에서 동일한 빌드 타입 강제, CI에서 Debug/Release 모두 테스트

---

## 4. 품질 게이트 (Quality Gates)

| 게이트 | 기준 | 상태 |
|--------|------|------|
| DummyAdapter 빌드 | uxdi_dummy.dll 생성 성공 | Pending |
| EmulAdapter 빌드 | uxdi_emul.dll 생성 성공 | Pending |
| DLL 로딩 | Factory를 통한 두 DLL 로딩 성공 | Pending |
| 단위 테스트 | 모든 AC 테스트 통과 | Pending |
| 시나리오 테스트 | 6개 built-in 시나리오 모두 통과 | Pending |
| 커버리지 | 85% 이상 | Pending |
| 메모리 누수 | IDetector 인스턴스 + DLL 핸들 누수 없음 | Pending |
| 벤더 SDK 독립 | uxdi_core + C++ stdlib만으로 빌드 성공 | Pending |

---

## 5. Definition of Done

- [ ] 모든 수락 기준(AC-DADP-01 ~ AC-DADP-08) 테스트 통과
- [ ] 모든 엣지 케이스(EC-DADP-01 ~ EC-DADP-05) 테스트 통과
- [ ] 6개 built-in 시나리오 모두 구현 및 테스트 통과
- [ ] 코드 커버리지 85% 이상
- [ ] 벤더 SDK 없이 독립 빌드 성공
- [ ] DLL Export 함수(CreateDetector/DestroyDetector) 정상 동작
- [ ] Factory를 통한 DLL 로딩 및 인스턴스 생성 성공
- [ ] 메모리 누수 없음
- [ ] Doxygen 주석 완성

---

## 추적성 태그

- **SPEC 참조**: SPEC-ADAPTER-001/spec.md
- **구현 계획**: SPEC-ADAPTER-001/plan.md
- **선행 SPEC**: SPEC-CORE-001
- **요구사항 매핑**: R-DADP-001 ~ R-DADP-012
