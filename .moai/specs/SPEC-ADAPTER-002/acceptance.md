# SPEC-ADAPTER-002: 수락 기준

**SPEC ID:** SPEC-ADAPTER-002
**제목:** Vendor SDK Adapters (Varex, Vieworks, ABYZ) - 수락 기준

---

## 1. 수락 기준 (Acceptance Criteria)

### AC-VADP-01: IDetector 인터페이스 완전 구현

**Given** VarexAdapter, VieworksAdapter, ABYZAdapter 각각이 빌드된 상태에서
**When** IDetector의 모든 순수 가상 메서드(Connect, Disconnect, GetState, Prepare, Fire, Acquire, GetFrame, AttachListener, DetachListener)를 호출하면
**Then** 모든 메서드가 벤더 SDK를 통해 정상 동작하거나, 적절한 에러 코드를 반환해야 한다.

### AC-VADP-02: Static Callback Bridge 동작 검증

**Given** 각 어댑터가 초기화된 상태에서
**When** 벤더 SDK로부터 C 스타일 콜백이 수신되면
**Then** Static Callback Bridge를 통해 올바른 C++ 인스턴스의 메서드가 호출되어야 하며, context 포인터가 정확한 인스턴스를 가리켜야 한다.

### AC-VADP-03: Varex 이미지 콜백 및 복사 검증

**Given** VarexAdapter가 Acquiring 상태에서
**When** Varex SDK에서 이미지 콜백이 수신되면
**Then** volatile 버퍼의 데이터가 새로운 shared_ptr<ImageFrame>으로 복사되고, mutex 보호 하에 등록된 모든 리스너의 OnImageReceived가 호출되어야 한다.

### AC-VADP-04: Vieworks 폴링-이벤트 변환 검증

**Given** VieworksAdapter가 초기화된 상태에서
**When** 폴링 스레드가 Vieworks SDK로부터 새 이미지를 감지하면
**Then** UXDI 이벤트 기반 콜백(OnImageReceived)으로 변환되어 리스너에게 전달되어야 한다.

### AC-VADP-05: 상태 매핑 검증

**Given** 각 어댑터가 연결된 상태에서
**When** 벤더 SDK의 내부 상태가 변경되면
**Then** 해당 상태가 올바른 UXDI DetectorState로 매핑되고, OnStateChanged 콜백이 호출되어야 한다.

### AC-VADP-06: 에러 코드 매핑 검증

**Given** 각 어댑터가 동작 중인 상태에서
**When** 벤더 SDK에서 에러가 발생하면
**Then** 벤더 에러 코드가 UXDI::ErrorCode로 매핑되어 OnError 콜백을 통해 전달되어야 하며, 벤더 고유 코드는 콘솔에 노출되지 않아야 한다.

### AC-VADP-07: DLL 팩토리 함수 검증

**Given** 각 어댑터 DLL이 빌드된 상태에서
**When** DetectorFactory가 해당 DLL을 로드하고 `CreateDetectorInstance()`를 호출하면
**Then** 유효한 IDetector 포인터가 반환되어야 하고, `DestroyDetectorInstance()`로 안전하게 해제되어야 한다.

### AC-VADP-08: 스레드 안전성 검증

**Given** 각 어댑터가 활성 상태(Prep/ReadyToFire/Acquiring/Processing)에서
**When** 복수 스레드에서 동시에 상태 조회, 이미지 수신, 설정 변경이 발생하면
**Then** std::mutex에 의해 데이터 경합(race condition)이 발생하지 않아야 한다.

---

## 2. 엣지 케이스 (Edge Cases)

### EC-VADP-01: SDK 초기화 실패

**Given** 벤더 SDK가 정상적으로 설치되지 않은 환경에서
**When** 어댑터의 Initialize를 호출하면
**Then** 적절한 UXDI::ErrorCode와 함께 OnError가 호출되고, 어댑터는 Disconnected 상태를 유지해야 한다.

### EC-VADP-02: 콜백 중 어댑터 소멸

**Given** 벤더 SDK 콜백이 진행 중인 상태에서
**When** 어댑터의 Disconnect 또는 소멸이 요청되면
**Then** 진행 중인 콜백이 안전하게 완료된 후 자원이 해제되어야 하며, dangling pointer가 발생하지 않아야 한다.

### EC-VADP-03: 지원하지 않는 설정 값

**Given** 어댑터가 초기화된 상태에서
**When** 벤더 SDK가 지원하지 않는 DetectorConfig 값이 전달되면
**Then** 적절한 에러 코드를 반환하고 기존 설정을 유지해야 한다.

### EC-VADP-04: Vieworks 폴링 스레드 종료

**Given** VieworksAdapter가 폴링 중인 상태에서
**When** Disconnect가 호출되면
**Then** 폴링 스레드가 안전하게 종료되고, 모든 스레드 자원이 해제되어야 한다.

### EC-VADP-05: 연속 Connect/Disconnect 사이클

**Given** 어댑터가 생성된 상태에서
**When** Connect와 Disconnect를 100회 반복하면
**Then** 메모리 누수가 발생하지 않아야 하며, 마지막 상태가 Disconnected여야 한다.

---

## 3. 위험 요인 (Risk Factors)

### RF-VADP-01: SDK 버퍼 관리 정책 불일치

- **위험:** 벤더 SDK의 버퍼 수명 주기가 문서와 다를 수 있음
- **검증 방법:** 실제 SDK로 버퍼 수명 주기 테스트 수행, volatile 여부 확인
- **대응:** Mandatory Copy를 기본 전략으로 하여 안전성 확보

### RF-VADP-02: 폴링 주기 최적화 어려움

- **위험:** Vieworks SDK의 폴링 주기가 이미지 프레임 속도와 맞지 않을 수 있음
- **검증 방법:** 다양한 폴링 주기에서의 프레임 드롭율 측정
- **대응:** 적응형 폴링 주기 구현 (프레임 속도에 따라 자동 조절)

### RF-VADP-03: ABYZ SDK 미확보에 따른 스켈레톤 상태 유지

- **위험:** ABYZ SDK를 적시에 확보하지 못해 완전한 구현이 불가할 수 있음
- **검증 방법:** SDK 확보 일정 추적 및 대체 계획 마련
- **대응:** Mock SDK 기반 스켈레톤 구현으로 인터페이스 준수 여부만 검증, SDK 확보 후 완전 구현

---

## 4. Definition of Done

- [ ] 모든 EARS 요구사항 (R-VADP-001 ~ R-VADP-012) 구현 완료
- [ ] 모든 수락 기준 (AC-VADP-01 ~ AC-VADP-08) 통과
- [ ] 엣지 케이스 (EC-VADP-01 ~ EC-VADP-05) 검증 완료
- [ ] Mock SDK 기반 단위 테스트 커버리지 85% 이상
- [ ] 3개 어댑터 DLL이 독립적으로 빌드 및 로드 성공
- [ ] clang-tidy 정적 분석 경고 0건
- [ ] 코드 리뷰 완료

---

## 5. 추적성 태그

- [SPEC-ADAPTER-002/acceptance] -> [SPEC-ADAPTER-002/spec]: EARS 요구사항별 수락 기준
- [SPEC-ADAPTER-002/acceptance] -> [SPEC-ADAPTER-002/plan]: 마일스톤별 검증 기준
- [SPEC-ADAPTER-002/acceptance] -> [SPEC-INTEG-001]: 통합 레벨 검증 위임
