# SPEC-CORE-001: 수락 기준 (Acceptance Criteria)

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-CORE-001 |
| **제목** | UXDI Core Framework - 수락 기준 |
| **검증 방법** | 단위 테스트 (Google Test) + 컴파일 검증 |

---

## 1. 수락 기준 (Acceptance Criteria)

### AC-CORE-01: IDetector 인터페이스 컴파일 검증

**Given** IDetector.h가 include/uxdi/ 디렉토리에 존재할 때
**When** 독립적으로 #include할 때
**Then** 컴파일 오류 없이 성공해야 한다
**And** 다음 순수 가상 메서드가 선언되어야 한다: Initialize, Connect, Disconnect, Prepare, SoftwareTrigger, Cancel, GetState, SetListener, SetParameter
**And** virtual ~IDetector() 소멸자가 선언되어야 한다
**And** 멤버 변수가 존재하지 않아야 한다

### AC-CORE-02: Types.h 타입 정의 검증

**Given** Types.h가 include/uxdi/ 디렉토리에 존재할 때
**When** ImageFrame 구조체를 인스턴스화할 때
**Then** width, height, depth, frameId, timestamp, data, dataSize 필드가 올바른 타입으로 존재해야 한다
**And** DetectorState enum이 7개 상태값(Disconnected, Idle, Prep, ReadyToFire, Acquiring, Processing, Error)을 포함해야 한다
**And** TriggerMode enum이 3개 값(Software, AED, ExternalSync)을 포함해야 한다
**And** ErrorCode enum이 정의된 모든 값을 포함해야 한다

### AC-CORE-03: IDetectorListener 콜백 인터페이스 검증

**Given** IDetectorListener.h가 include/uxdi/ 디렉토리에 존재할 때
**When** 리스너 클래스가 IDetectorListener를 상속받을 때
**Then** OnStateChanged, OnImageReceived, OnError, OnTemperatureUpdate 4개 메서드를 구현해야 한다
**And** 순수 가상 인터페이스로만 구성되어야 한다 (멤버 변수 없음)

### AC-CORE-04: DetectorFactory DLL 로딩 성공

**Given** 유효한 어댑터 DLL 파일이 지정된 경로에 존재할 때
**When** DetectorFactory::LoadAdapter(dllPath)를 호출할 때
**Then** shared_ptr<IDetector>가 nullptr가 아닌 유효한 인스턴스로 반환되어야 한다
**And** 반환된 IDetector의 모든 순수 가상 메서드가 호출 가능해야 한다

### AC-CORE-05: DetectorFactory DLL 로딩 실패 처리

**Given** 존재하지 않는 DLL 경로가 주어졌을 때
**When** DetectorFactory::LoadAdapter(invalidPath)를 호출할 때
**Then** std::runtime_error가 throw되어야 한다
**And** 에러 메시지에 DLL 파일 경로가 포함되어야 한다

**Given** CreateDetector 심볼이 없는 DLL이 주어졌을 때
**When** DetectorFactory::LoadAdapter(dllPath)를 호출할 때
**Then** std::runtime_error가 throw되어야 한다
**And** 에러 메시지에 실패한 심볼 이름이 포함되어야 한다

### AC-CORE-06: 상태 머신 전이 규칙 검증

**Given** 디텍터가 Disconnected 상태일 때
**When** Prepare()를 호출할 때
**Then** 등록된 리스너의 OnError 콜백이 호출되어야 한다

**Given** 디텍터가 Idle 상태일 때
**When** Prepare()를 호출할 때
**Then** 상태가 Prep으로 전이되어야 한다
**And** 등록된 리스너의 OnStateChanged(Prep) 콜백이 호출되어야 한다

**Given** 디텍터가 ReadyToFire 상태이고 TriggerMode가 Software일 때
**When** SoftwareTrigger()를 호출할 때
**Then** 상태가 Acquiring으로 전이되어야 한다

### AC-CORE-07: DetectorManager 생명주기 관리 검증

**Given** DetectorManager 인스턴스가 존재할 때
**When** RegisterDetector("test", detector)를 호출한 후 GetDetector("test")를 호출할 때
**Then** 등록된 동일한 디텍터 인스턴스가 반환되어야 한다

**Given** 여러 디텍터가 등록되어 있을 때
**When** ShutdownAll()을 호출할 때
**Then** 모든 등록된 디텍터의 Disconnect()가 호출되어야 한다
**And** 내부 레지스트리가 비워져야 한다

### AC-CORE-08: ABI 안정성 검증

**Given** IDetector 인터페이스가 정의되어 있을 때
**When** 인터페이스를 정적 분석할 때
**Then** 멤버 변수가 0개여야 한다
**And** 모든 public 메서드가 virtual 키워드를 포함해야 한다
**And** = 0 (순수 가상) 또는 = default (소멸자)만 허용되어야 한다

---

## 2. 엣지 케이스 (Edge Cases)

### EC-CORE-01: DLL 핸들 누수 방지

**Given** DetectorFactory에서 DLL을 로드한 후
**When** 반환된 shared_ptr<IDetector>가 소멸될 때
**Then** DestroyDetector가 먼저 호출된 후 FreeLibrary가 호출되어야 한다
**And** DLL 핸들 누수가 발생하지 않아야 한다

### EC-CORE-02: 동일 DLL 중복 로딩

**Given** 동일한 DLL 경로로 LoadAdapter를 두 번 호출할 때
**When** 두 번째 호출이 완료되면
**Then** 독립적인 두 개의 IDetector 인스턴스가 반환되어야 한다
**And** 각 인스턴스의 소멸이 독립적으로 처리되어야 한다

### EC-CORE-03: Manager에 등록되지 않은 이름 조회

**Given** DetectorManager에 "test" 이름이 등록되지 않았을 때
**When** GetDetector("test")를 호출할 때
**Then** nullptr 또는 빈 shared_ptr이 반환되어야 한다
**And** 예외가 발생하지 않아야 한다

### EC-CORE-04: SoftwareTrigger 상태 위반

**Given** 디텍터가 Idle 상태일 때
**When** SoftwareTrigger()를 호출할 때
**Then** OnError(400, "Detector Not Ready") 콜백이 호출되어야 한다
**And** 디텍터 상태가 Idle에서 변경되지 않아야 한다

### EC-CORE-05: 리스너 미등록 시 상태 전이

**Given** IDetectorListener가 등록되지 않은 디텍터에서
**When** 상태 전이가 발생할 때
**Then** 상태는 정상적으로 전이되어야 한다
**And** 예외나 crash가 발생하지 않아야 한다

---

## 3. 위험 요소 (Risk Factors)

### RF-CORE-01: vtable 호환성

**위험**: 서로 다른 MSVC 버전으로 빌드된 Core와 어댑터 DLL 간 vtable 레이아웃 불일치로 인한 런타임 crash
**영향도**: High
**검증 방법**: CI 파이프라인에서 동일 컴파일러 버전 강제, ABI 호환성 테스트 추가

### RF-CORE-02: DLL 언로딩 순서

**위험**: IDetector 인스턴스가 해제되기 전에 DLL이 FreeLibrary로 언로드되면 가상 함수 테이블이 무효화되어 crash 발생
**영향도**: High
**검증 방법**: shared_ptr custom deleter 테스트, 소멸 순서 검증 테스트

### RF-CORE-03: 멀티스레드 Factory 접근

**위험**: 여러 스레드에서 동시에 LoadAdapter를 호출하면 경합 조건 발생 가능
**영향도**: Medium
**검증 방법**: 스레드 안전성 단위 테스트, ThreadSanitizer 활용

---

## 4. 품질 게이트 (Quality Gates)

| 게이트 | 기준 | 상태 |
|--------|------|------|
| 컴파일 | 모든 헤더 독립 컴파일 성공 | Pending |
| 단위 테스트 | 모든 AC 테스트 통과 | Pending |
| 커버리지 | 85% 이상 | Pending |
| ABI 검증 | 멤버 변수 0개 확인 | Pending |
| 메모리 누수 | DLL 핸들 + IDetector 인스턴스 누수 없음 | Pending |
| 코드 스타일 | Google C++ Style Guide 준수 | Pending |

---

## 5. Definition of Done

- [ ] 모든 수락 기준(AC-CORE-01 ~ AC-CORE-08) 테스트 통과
- [ ] 모든 엣지 케이스(EC-CORE-01 ~ EC-CORE-05) 테스트 통과
- [ ] 코드 커버리지 85% 이상
- [ ] ABI 안정성 규칙 위반 없음
- [ ] 메모리 누수 없음 (DLL 핸들, IDetector 인스턴스)
- [ ] 빌드 시스템(CMake) 정상 동작
- [ ] Doxygen 주석 완성

---

## 추적성 태그

- **SPEC 참조**: SPEC-CORE-001/spec.md
- **구현 계획**: SPEC-CORE-001/plan.md
- **요구사항 매핑**: R-CORE-001 ~ R-CORE-012
