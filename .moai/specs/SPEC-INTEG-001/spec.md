# SPEC-INTEG-001: Integration, Error Handling, Performance, CI/CD

**SPEC ID:** SPEC-INTEG-001
**제목:** Integration, Error Handling, Performance, CI/CD
**Phase:** 3 (W6-7)
**우선순위:** Medium
**상태:** Planned
**생성일:** 2026-02-16
**의존성:** SPEC-CORE-001, SPEC-ADAPTER-001, SPEC-ADAPTER-002

---

## 1. 개요

UXDI 프로젝트의 최종 통합 단계입니다. 콘솔 GUI 샘플 애플리케이션(CLI 우선), 포괄적인 에러 처리 및 복구 메커니즘, Zero-Copy 파이프라인 성능 검증, CI/CD 파이프라인 구축을 포함합니다.

이 단계에서는 Phase 1(SPEC-CORE-001)과 Phase 2(SPEC-ADAPTER-001, SPEC-ADAPTER-002)에서 구현된 모든 컴포넌트를 통합하고, 프로덕션 수준의 품질과 성능을 검증합니다.

---

## 2. EARS 요구사항

### R-INTG-001: 콘솔 샘플 애플리케이션

시스템은 **항상** 전체 UXDI 워크플로우(어댑터 선택 -> Connect -> Prepare -> Fire -> Acquire -> GetFrame -> Disconnect)를 시연하는 콘솔 GUI 샘플 애플리케이션(Qt 또는 WinForms)을 제공해야 한다.

### R-INTG-002: 동적 어댑터 선택

**WHEN** GUI에서 사용자가 어댑터를 선택하면 **THEN** DetectorFactory가 해당 DLL을 동적으로 로드하고, 애플리케이션 재시작 없이 어댑터를 전환해야 한다.

### R-INTG-003: Zero-Copy 파이프라인 성능 검증

시스템은 **항상** 2048x2048 16-bit 이미지 10fps 기준에서 복사 기반(copy baseline) 대비 75% 이상의 메모리 대역폭 감소를 달성해야 한다.

### R-INTG-004: UXDI::ErrorCode 열거형 완성

시스템은 **항상** 하드웨어 에러(HardwareNotFound, CableDisconnected), 운영 에러(CalibrationRequired, TemperatureError), 시스템 에러(MemoryAllocation, DLLLoadFailure)를 포괄하는 UXDI::ErrorCode 열거형을 제공해야 한다.

### R-INTG-005: 케이블 분리 복구

**IF** 이미지 수집 중 케이블이 분리되면 **THEN** 시스템은 Error 상태로 전환하고, OnError(CableDisconnected)를 호출하며, SDK 자원을 안전하게 해제하고, 전체 재초기화 없이 재연결을 허용해야 한다.

### R-INTG-006: SDK 크래시/무응답 처리

**IF** 벤더 SDK가 크래시하거나 무응답 상태가 되면 **THEN** 시스템은 타임아웃을 감지하고, Error 상태로 전환하며, 안전한 어댑터 셧다운을 수행하고, 새로운 어댑터 인스턴스 생성을 허용해야 한다.

### R-INTG-007: 벤더별 에러 코드 매핑 테이블

각 벤더의 문서화된 모든 에러 코드는 **항상** UXDI::ErrorCode로 매핑되어야 하며, 매핑되지 않은 에러 코드에 대해서는 "Unknown" 폴백을 제공해야 한다.

### R-INTG-008: 이미지 수집 레이턴시

**WHEN** SDK 콜백이 수신되면 **THEN** OnImageReceived 전달까지의 레이턴시가 50ms 미만이어야 한다.

### R-INTG-009: 프레임 드롭 없는 지속 수집

시스템은 **항상** 2048x2048 16-bit 이미지를 10fps로 60초간 지속 수집할 때 프레임 드롭이 0이어야 한다.

### R-INTG-010: CI/CD 파이프라인

시스템은 **항상** Dummy+Emul 어댑터를 빌드하고, 모든 테스트를 실행하며, 코드 커버리지 85% 이상을 리포트하는 CI/CD 파이프라인을 제공해야 한다.

### R-INTG-011: 메모리 누수 제로

시스템은 1,000회 acquire-release 사이클 후 메모리 누수가 **발생하지 않아야 한다**.

### R-INTG-012: 성능 목표

시스템은 **항상** 다음 성능 목표를 달성해야 한다: CPU 유휴 시 5% 미만, 메모리 사용량은 활성 프레임에 비례, mutex 경합 1ms 미만.

---

## 3. 아키텍처

### 3.1 콘솔 애플리케이션

- **Phase 1 산출물:** CLI 기반 샘플 (cli_sample.cpp)
  - 명령줄 인자로 어댑터 선택
  - 텍스트 기반 상태 표시
  - 이미지 수집 및 저장 시연
- **Phase 3 옵션:** GUI 확장 (Qt 또는 WinForms)
  - 어댑터 선택 드롭다운
  - 실시간 이미지 미리보기
  - 상태 머신 시각화

### 3.2 멀티 디텍터 관리

- **DetectorManager:** `map<string, shared_ptr<IDetector>>`를 사용한 다중 디텍터 인스턴스 관리
- 각 디텍터에 대해 독립적인 생명주기 관리
- 동시 다중 어댑터 운영 지원

### 3.3 에러 복구 전략

- **지수 백오프 재연결:** 1초 -> 2초 -> 4초, 최대 3회 시도
- **안전한 셧다운:** SDK 크래시 시 타임아웃 기반 감지 -> Error 상태 전환 -> 자원 해제
- **부분 재초기화:** 케이블 재연결 시 전체 재초기화 없이 Connect() 재호출

### 3.4 CI/CD 파이프라인

- **빌드:** CMake + CTest
- **커버리지:** lcov (Linux) / OpenCppCoverage (Windows)
- **정적 분석:** clang-tidy
- **대상:** Dummy + Emul 어댑터 (벤더 SDK 불필요)

---

## 4. 비기능 요구사항 테이블

| 항목 | 목표 | 측정 방법 |
|------|------|----------|
| 메모리 대역폭 감소 | 75% 이상 | Zero-Copy vs Copy 기반 벤치마크 비교 |
| 이미지 수집 레이턴시 | < 50ms | SDK 콜백 -> OnImageReceived 시간 측정 |
| 프레임 드롭 | 0 | 10fps 60초 지속 수집 테스트 |
| 코드 커버리지 | 85% 이상 | lcov / OpenCppCoverage |
| CPU 유휴 사용량 | < 5% | 프로파일러 측정 |
| 메모리 누수 | 0 | 1,000회 acquire-release 사이클 후 valgrind/DrMemory |
| Mutex 경합 | < 1ms | 락 경합 프로파일링 |
| DLL 로드 시간 | < 500ms | 어댑터 DLL 로드 시간 측정 |

---

## 5. 파일 분해

### 신규 생성 파일

| 파일 경로 | 예상 라인 수 | 설명 |
|-----------|-------------|------|
| `samples/cli_sample.cpp` | ~120L | CLI 기반 샘플 애플리케이션 |
| `tests/unit/test_factory.cpp` | ~100L | DetectorFactory 단위 테스트 |
| `tests/unit/test_manager.cpp` | ~100L | DetectorManager 단위 테스트 |
| `tests/unit/test_dummy_adapter.cpp` | ~80L | DummyAdapter 단위 테스트 |
| `tests/unit/test_emul_adapter.cpp` | ~120L | EmulAdapter 단위 테스트 |
| `tests/unit/test_error_mapping.cpp` | ~150L | 에러 코드 매핑 단위 테스트 |
| `tests/unit/test_state_machine.cpp` | ~100L | 상태 머신 단위 테스트 |
| `tests/integration/test_lifecycle.cpp` | ~150L | 어댑터 생명주기 통합 테스트 |
| `tests/integration/test_multi_detector.cpp` | ~120L | 멀티 디텍터 통합 테스트 |
| `tests/integration/test_zero_copy.cpp` | ~100L | Zero-Copy 파이프라인 성능 테스트 |
| `.github/workflows/ci.yml` | ~60L | CI/CD 파이프라인 설정 |
| `tests/CMakeLists.txt` | ~40L | 테스트 빌드 설정 |

**총계:** 12개 신규 파일, 약 1,240줄 (프로덕션 + 테스트 코드)

---

## 6. 의존성

### SPEC 의존성 트리

```
SPEC-CORE-001 (의존성 없음)
    |
    +-> SPEC-ADAPTER-001 (CORE-001에 의존)
    |       |
    |       +-> SPEC-ADAPTER-002 (CORE-001, ADAPTER-001에 의존)
    |
    +-> SPEC-INTEG-001 (CORE-001, ADAPTER-001, ADAPTER-002에 의존)  <-- 현재 SPEC
```

### 기술적 의존성

- **SPEC-CORE-001:** IDetector, DetectorFactory, DetectorManager, DetectorState, ImageFrame, ErrorCode 등 모든 핵심 타입 및 인터페이스
- **SPEC-ADAPTER-001:** DummyAdapter, EmulAdapter - 통합 테스트 및 CI/CD에서 하드웨어 없이 사용
- **SPEC-ADAPTER-002:** VarexAdapter, VieworksAdapter, ABYZAdapter - 실제 벤더 어댑터 통합 테스트 대상

---

## 7. 기술적 위험 요소

| 위험 | 영향도 | 발생 가능성 | 대응 방안 |
|------|--------|------------|----------|
| **GUI 프레임워크 라이선스:** Qt의 상용 라이선스 비용이 프로젝트 예산을 초과할 수 있음 | Medium | Medium | CLI 우선 구현, GUI는 선택적 확장으로 계획. 오픈소스 대안(WinForms, ImGui) 평가 |
| **성능 목표 변동성:** 하드웨어 환경에 따라 성능 목표 달성이 어려울 수 있음 | High | Low | 기준 하드웨어 사양 명시, 상대적 성능 비교(baseline 대비 개선율) 사용 |
| **벤더 SDK 없는 CI:** CI/CD 환경에서 벤더 SDK를 사용할 수 없음 | Low | High | Dummy + Emul 어댑터 기반 테스트로 CI/CD 구성 |

---

## 8. 가정 사항

- **플랫폼:** Windows를 1차 타겟으로 함 (Linux 지원은 향후)
- **SDK 유형:** C/C++ SDK API만 지원 (COM, .NET 기반 SDK는 제외)
- **인스턴스 정책:** 어댑터당 단일 인스턴스 (동일 벤더의 다중 디텍터는 다중 인스턴스로 처리)
- **이미지 형식:** 16-bit 그레이스케일만 지원 (컬러 이미지는 향후)
- **프로세스 모델:** 단일 프로세스 (멀티 프로세스 격리는 향후)

---

## 9. 추적성 태그

- [SPEC-INTEG-001] -> [SPEC-CORE-001]: 핵심 인터페이스 통합 검증
- [SPEC-INTEG-001] -> [SPEC-ADAPTER-001]: Dummy/Emul 기반 테스트
- [SPEC-INTEG-001] -> [SPEC-ADAPTER-002]: 벤더 어댑터 통합 테스트
