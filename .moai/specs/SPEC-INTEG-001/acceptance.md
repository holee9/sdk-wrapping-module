# SPEC-INTEG-001: 수락 기준

**SPEC ID:** SPEC-INTEG-001
**제목:** Integration, Error Handling, Performance, CI/CD - 수락 기준

---

## 1. 수락 기준 (Acceptance Criteria)

### AC-INTG-01: CLI 샘플 전체 워크플로우

**Given** CLI 샘플 애플리케이션이 빌드된 상태에서
**When** EmulAdapter를 지정하여 실행하면
**Then** Connect -> Prepare -> Fire -> Acquire -> GetFrame -> Disconnect 전체 워크플로우가 정상 완료되고, 각 단계의 상태 변화가 텍스트로 출력되어야 한다.

### AC-INTG-02: 동적 어댑터 로딩

**Given** DetectorFactory가 초기화된 상태에서
**When** 설정에서 지정된 어댑터 DLL을 로드하면
**Then** 500ms 이내에 DLL이 로드되고, 유효한 IDetector 인스턴스가 생성되어야 하며, 애플리케이션 재시작 없이 어댑터 전환이 가능해야 한다.

### AC-INTG-03: Zero-Copy 메모리 대역폭 검증

**Given** 2048x2048 16-bit 이미지를 10fps로 생성하는 테스트 환경에서
**When** shared_ptr 기반 Zero-Copy 파이프라인과 복사 기반 파이프라인을 비교하면
**Then** Zero-Copy 파이프라인의 메모리 대역폭이 복사 기반 대비 75% 이상 감소해야 한다.

### AC-INTG-04: 에러 코드 완전성

**Given** UXDI::ErrorCode 열거형이 정의된 상태에서
**When** 하드웨어 에러(HardwareNotFound, CableDisconnected), 운영 에러(CalibrationRequired, TemperatureError), 시스템 에러(MemoryAllocation, DLLLoadFailure)를 포함하는지 검증하면
**Then** 모든 카테고리의 에러 코드가 열거형에 포함되어 있어야 한다.

### AC-INTG-05: 케이블 분리 복구 시나리오

**Given** EmulAdapter가 Acquiring 상태에서
**When** 케이블 분리 에러가 주입되면
**Then** 시스템은 Error 상태로 전환하고, OnError(CableDisconnected)를 호출하며, SDK 자원을 해제하고, 이후 Disconnect -> Connect 시퀀스로 재연결이 가능해야 한다.

### AC-INTG-06: 프레임 드롭 없는 지속 수집

**Given** EmulAdapter가 10fps로 2048x2048 16-bit 이미지를 생성하는 상태에서
**When** 60초간 지속 수집을 수행하면
**Then** 수신된 프레임 수가 600(+/-1)이어야 하며, 프레임 드롭이 0이어야 한다.

### AC-INTG-07: 메모리 누수 제로 검증

**Given** DummyAdapter 또는 EmulAdapter가 초기화된 상태에서
**When** 1,000회 acquire-release 사이클을 수행하면
**Then** 사이클 전후 메모리 사용량 차이가 허용 범위(< 1MB) 이내여야 하며, valgrind/DrMemory 리포트에서 누수가 0이어야 한다.

### AC-INTG-08: CI/CD 파이프라인 동작

**Given** GitHub Actions CI/CD 워크플로우가 설정된 상태에서
**When** main 브랜치에 push 또는 PR이 생성되면
**Then** Windows 환경에서 CMake 빌드, Dummy+Emul 어댑터 컴파일, CTest 실행, 커버리지 리포트(85% 이상)가 자동으로 수행되어야 한다.

---

## 2. 엣지 케이스 (Edge Cases)

### EC-INTG-01: 존재하지 않는 DLL 로드 시도

**Given** DetectorFactory가 초기화된 상태에서
**When** 존재하지 않는 어댑터 DLL 경로가 지정되면
**Then** DLLLoadFailure 에러 코드와 함께 OnError가 호출되고, 시스템은 안정적인 상태를 유지해야 한다.

### EC-INTG-02: 동시 다중 어댑터 운영

**Given** DetectorManager에 DummyAdapter와 EmulAdapter가 등록된 상태에서
**When** 두 어댑터에서 동시에 이미지 수집을 수행하면
**Then** 각 어댑터가 독립적으로 동작하며, 상호 간섭 없이 이미지를 수집해야 한다.

### EC-INTG-03: 지수 백오프 재연결 최대 횟수 초과

**Given** 어댑터가 Error 상태에서 재연결을 시도하는 상태에서
**When** 3회 재연결 시도가 모두 실패하면
**Then** 재연결을 중단하고, 최종 실패 에러 코드와 함께 OnError를 호출해야 한다.

### EC-INTG-04: 빈 이미지 프레임 수신

**Given** 어댑터가 Acquiring 상태에서
**When** SDK에서 크기가 0인 이미지 데이터를 반환하면
**Then** 적절한 에러 코드와 함께 해당 프레임을 무시하고, 다음 프레임 수집을 계속해야 한다.

### EC-INTG-05: CLI 샘플 잘못된 인자

**Given** CLI 샘플 애플리케이션이 빌드된 상태에서
**When** 지원되지 않는 어댑터 이름이나 잘못된 인자가 전달되면
**Then** 사용법 안내 메시지를 출력하고, 적절한 종료 코드로 종료해야 한다.

---

## 3. 위험 요인 (Risk Factors)

### RF-INTG-01: 하드웨어 환경 의존적 성능 측정

- **위험:** 성능 측정 결과가 테스트 환경(CPU, 메모리, 디스크)에 크게 의존
- **검증 방법:** 기준 하드웨어 사양 명시, 상대적 성능 비교(baseline 대비 개선율) 사용
- **대응:** 절대적 수치보다 상대적 개선율 중심으로 수락 기준 평가

### RF-INTG-02: EmulAdapter 시나리오의 실제 SDK 동작 차이

- **위험:** EmulAdapter의 에러 시뮬레이션이 실제 벤더 SDK의 에러 동작과 다를 수 있음
- **검증 방법:** 가용한 벤더 SDK로 에러 시나리오 재검증
- **대응:** EmulAdapter 시나리오를 보수적으로 설계하여 실제 SDK보다 까다로운 조건으로 테스트

### RF-INTG-03: CI/CD 환경에서의 OpenCppCoverage 호환성

- **위험:** GitHub Actions의 Windows 러너에서 OpenCppCoverage가 정상 동작하지 않을 수 있음
- **검증 방법:** CI 파이프라인 초기 설정 시 커버리지 도구 호환성 검증
- **대응:** 대안 도구(gcov + MinGW, lcov + WSL) 평가 및 폴백 계획

---

## 4. Definition of Done

- [ ] 모든 EARS 요구사항 (R-INTG-001 ~ R-INTG-012) 구현 완료
- [ ] 모든 수락 기준 (AC-INTG-01 ~ AC-INTG-08) 통과
- [ ] 엣지 케이스 (EC-INTG-01 ~ EC-INTG-05) 검증 완료
- [ ] CLI 샘플이 EmulAdapter로 전체 워크플로우 시연 성공
- [ ] 단위 테스트 + 통합 테스트 커버리지 85% 이상
- [ ] CI/CD 파이프라인이 push/PR에서 자동 실행
- [ ] Zero-Copy 벤치마크: 메모리 대역폭 75% 이상 감소 확인
- [ ] 1,000회 acquire-release 사이클에서 메모리 누수 0 확인
- [ ] clang-tidy 정적 분석 경고 0건
- [ ] 코드 리뷰 완료

---

## 5. 추적성 태그

- [SPEC-INTEG-001/acceptance] -> [SPEC-INTEG-001/spec]: EARS 요구사항별 수락 기준
- [SPEC-INTEG-001/acceptance] -> [SPEC-INTEG-001/plan]: 마일스톤별 검증 기준
- [SPEC-INTEG-001/acceptance] -> [SPEC-CORE-001]: 핵심 프레임워크 통합 검증
- [SPEC-INTEG-001/acceptance] -> [SPEC-ADAPTER-001]: Dummy/Emul 기반 시나리오 검증
- [SPEC-INTEG-001/acceptance] -> [SPEC-ADAPTER-002]: 벤더 어댑터 통합 검증
