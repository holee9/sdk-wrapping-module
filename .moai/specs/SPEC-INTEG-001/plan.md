# SPEC-INTEG-001: 구현 계획

**SPEC ID:** SPEC-INTEG-001
**제목:** Integration, Error Handling, Performance, CI/CD - 구현 계획
**Phase:** 3 (W6-7)

---

## 1. 구현 순서

### 마일스톤 1: CLI 샘플 애플리케이션 (Primary Goal)

**우선순위:** High - 전체 UXDI 워크플로우를 시연하는 기본 산출물

**구현 항목:**
1. `samples/cli_sample.cpp` 작성
   - 명령줄 인자 파서 (어댑터 이름, 설정 파일 경로)
   - DetectorFactory를 통한 어댑터 동적 로딩
   - 전체 촬영 워크플로우 시연 (Connect -> Prepare -> Fire -> Acquire -> GetFrame -> Disconnect)
   - 텍스트 기반 상태 표시 및 이미지 메타데이터 출력
   - 에러 처리 시연 (에러 콜백 수신 및 복구)
2. CLI 샘플용 CMakeLists.txt 빌드 설정
3. 사용법 문서 (README 또는 인라인 주석)

### 마일스톤 2: 포괄적 테스트 스위트 (Primary Goal)

**우선순위:** High - 품질 보증의 핵심

**구현 항목:**
1. **단위 테스트:**
   - `test_factory.cpp`: DLL 로딩, 팩토리 함수 호출, 인스턴스 생성/소멸
   - `test_manager.cpp`: 멀티 디텍터 관리, 생명주기 관리
   - `test_dummy_adapter.cpp`: DummyAdapter IDetector 인터페이스 검증
   - `test_emul_adapter.cpp`: EmulAdapter 시나리오 기반 테스트 (정상/에러/지연)
   - `test_error_mapping.cpp`: 벤더별 에러 코드 -> UXDI::ErrorCode 매핑 검증
   - `test_state_machine.cpp`: 상태 전이 규칙 검증, 잘못된 전이 거부
2. **통합 테스트:**
   - `test_lifecycle.cpp`: 어댑터 생성 -> 초기화 -> 촬영 -> 정리 -> 소멸 전체 사이클
   - `test_multi_detector.cpp`: 복수 어댑터 동시 운영, 독립적 생명주기
   - `test_zero_copy.cpp`: shared_ptr 참조 카운트 검증, 메모리 복사 발생 여부 확인
3. `tests/CMakeLists.txt` 테스트 빌드 설정

### 마일스톤 3: CI/CD 파이프라인 (Secondary Goal)

**우선순위:** Medium - 자동화된 품질 보증

**구현 항목:**
1. `.github/workflows/ci.yml` GitHub Actions 설정
   - Windows 빌드 (MSVC 2022)
   - CMake 빌드 및 CTest 실행
   - Dummy + Emul 어댑터만 빌드 (벤더 SDK 불필요)
   - 코드 커버리지 리포트 (OpenCppCoverage)
   - clang-tidy 정적 분석
2. 커버리지 뱃지 및 리포트 설정

### 마일스톤 4: 에러 복구 메커니즘 강화 (Secondary Goal)

**우선순위:** Medium - 프로덕션 안정성

**구현 항목:**
1. UXDI::ErrorCode 열거형 완성 (하드웨어/운영/시스템 에러 분류)
2. 지수 백오프 재연결 로직 구현 (1s -> 2s -> 4s, 최대 3회)
3. SDK 크래시/타임아웃 감지 로직 구현
4. 케이블 분리 시나리오 테스트 (EmulAdapter 기반)
5. 에러 복구 시나리오 통합 테스트

### 마일스톤 5: GUI 확장 (Optional Goal)

**우선순위:** Low - Phase 3 시간 여유 시 추진

**구현 항목:**
1. GUI 프레임워크 선택 (Qt / WinForms / ImGui)
2. 어댑터 선택 UI
3. 실시간 이미지 미리보기
4. 상태 머신 시각화

---

## 2. 기술 접근 방식

### 개발 방법론

**Hybrid 모드** (quality.yaml development_mode: hybrid)

- **신규 코드 (테스트, CLI 앱, CI 설정):** TDD 접근 - 테스트를 먼저 작성하고, 구현 코드 완성
- **기존 코드 (에러 처리 강화, 성능 최적화):** DDD 접근 - 기존 동작을 분석(ANALYZE)하고 보존(PRESERVE)한 후 개선(IMPROVE)

### EmulAdapter 기반 시나리오 테스트

EmulAdapter의 시나리오 엔진을 활용하여 실제 하드웨어 없이 다양한 에러 시나리오를 검증합니다:

- **정상 촬영 시나리오:** Connect -> Prepare -> Fire -> Acquire -> GetFrame
- **케이블 분리 시나리오:** Acquiring 중 CableDisconnected 에러 주입
- **SDK 크래시 시나리오:** 타임아웃 기반 무응답 감지
- **재연결 시나리오:** Error -> Disconnect -> Connect 복구 플로우

---

## 3. 파일 규모

| 범주 | 파일 수 | 예상 라인 수 |
|------|---------|-------------|
| CLI 샘플 | 1 | ~120L |
| 단위 테스트 | 6 | ~650L |
| 통합 테스트 | 3 | ~370L |
| CI/CD 설정 | 1 | ~60L |
| 테스트 CMake | 1 | ~40L |
| **합계** | **12** | **~1,240L** |

---

## 4. 위험 대응 계획

### 위험 1: GUI 프레임워크 라이선스

- **대응:** CLI 우선 구현으로 핵심 기능 시연 보장. GUI는 오픈소스 대안(ImGui, WinForms) 평가 후 결정
- **영향:** GUI 없이도 UXDI 전체 워크플로우 시연 가능

### 위험 2: 성능 목표 환경 의존성

- **대응:** 기준 하드웨어 사양 명시, 상대적 성능 비교(baseline 대비 개선율) 중심으로 검증
- **영향:** 절대적 성능 수치보다 상대적 개선율에 초점

### 위험 3: CI에서 벤더 SDK 부재

- **대응:** Dummy + Emul 어댑터 기반으로 CI/CD 구성, 벤더 어댑터 테스트는 로컬/전용 환경에서 수행
- **영향:** CI에서의 테스트 범위는 핵심 프레임워크 + 시뮬레이터로 제한

---

## 5. 추적성 태그

- [SPEC-INTEG-001/plan] -> [SPEC-INTEG-001/spec]: EARS 요구사항 구현 계획
- [SPEC-INTEG-001/plan] -> [SPEC-CORE-001]: 핵심 프레임워크 통합
- [SPEC-INTEG-001/plan] -> [SPEC-ADAPTER-001]: Dummy/Emul 기반 테스트
- [SPEC-INTEG-001/plan] -> [SPEC-ADAPTER-002]: 벤더 어댑터 통합 대상
