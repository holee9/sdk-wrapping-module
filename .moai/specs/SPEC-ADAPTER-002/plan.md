# SPEC-ADAPTER-002: 구현 계획

**SPEC ID:** SPEC-ADAPTER-002
**제목:** Vendor SDK Adapters (Varex, Vieworks, ABYZ) - 구현 계획
**Phase:** 2 (W3-5)

---

## 1. 구현 순서

### 마일스톤 1: VarexAdapter (Primary Goal)

**우선순위:** High - 가장 잘 문서화된 SDK, 첫 번째 실제 벤더 어댑터

**구현 항목:**
1. Varex SDK 분석 및 API 매핑 문서 작성
2. VarexDetector.h 클래스 선언 (IDetector 구현 + Varex 전용 멤버)
3. Static Callback Bridge 구현 (C API -> C++ 인스턴스)
4. VarexSDK::Init 래핑 및 초기화 시퀀스 구현
5. 상태 매핑 (VAREX_IDLE -> Idle 등) 구현
6. 에러 코드 매핑 (Varex 에러 -> UXDI::ErrorCode) 구현
7. 이미지 콜백 처리 (volatile 버퍼 복사 + ImageFrame 래핑)
8. Config Mapping (DetectorConfig -> VarexConfig) 구현
9. VarexAdapter.cpp DLL Export 팩토리 함수 구현
10. CMakeLists.txt Varex 빌드 설정
11. Mock Varex SDK 레이어 작성 (단위 테스트용)
12. 단위 테스트 작성 및 검증

### 마일스톤 2: VieworksAdapter (Secondary Goal)

**우선순위:** High - 폴링 기반 SDK로 VarexAdapter와 다른 패턴

**구현 항목:**
1. Vieworks SDK 분석 및 폴링 API 매핑
2. VieworksDetector.h 클래스 선언 (폴링 스레드 멤버 포함)
3. 폴링-이벤트 변환 전용 스레드 구현
4. 폴링 주기 최적화 및 스레드 안전 종료 로직
5. 상태 매핑 (VW_STATE_STANDBY -> Idle 등) 구현
6. 에러 코드 매핑 구현
7. Zero-Copy 가능 여부 평가 및 이미지 처리 구현
8. Config Mapping 구현
9. VieworksAdapter.cpp DLL Export 팩토리 함수 구현
10. CMakeLists.txt Vieworks 빌드 설정
11. Mock Vieworks SDK 레이어 작성
12. 단위 테스트 작성 및 검증

### 마일스톤 3: ABYZAdapter (Tertiary Goal)

**우선순위:** Medium - SDK API 스타일 미확정, 스켈레톤으로 시작

**구현 항목:**
1. ABYZ SDK 분석 (가용 시)
2. ABYZDetector.h 클래스 선언 (TODO 마커 포함)
3. 기본 IDetector 구현 (Callback Bridge, State Mapping 스켈레톤)
4. 에러 코드 매핑 (기본 매핑 + Unknown 폴백)
5. 이미지 처리 (Default Copy 전략)
6. ABYZAdapter.cpp DLL Export 팩토리 함수 구현
7. CMakeLists.txt ABYZ 빌드 설정
8. Mock ABYZ SDK 레이어 작성
9. 단위 테스트 작성 (스켈레톤 수준)

### 마일스톤 4: 공통 인프라 (Supporting Goal)

**우선순위:** High - 전체 벤더 어댑터 빌드 인프라

**구현 항목:**
1. `cmake/FindVendorSDK.cmake` 벤더 SDK 자동 검색 모듈
2. 조건부 CMake 빌드 설정 (SDK 미설치 시 어댑터 빌드 자동 스킵)
3. Mock SDK 공통 인프라 (테스트용)

---

## 2. 기술 접근 방식

### 개발 방법론

**Hybrid 모드** (quality.yaml development_mode: hybrid)

- **신규 코드 (어댑터 구현):** TDD 접근 - Mock SDK 기반 테스트를 먼저 작성하고, 어댑터를 구현
- **벤더 SDK 래핑 (기존 SDK 분석):** DDD 접근 - SDK 동작을 분석(ANALYZE)하고, 동작을 보존(PRESERVE)하며, UXDI 인터페이스로 개선(IMPROVE)

### Mock SDK 레이어 전략

실제 벤더 SDK 없이도 단위 테스트가 가능하도록 Mock SDK 레이어를 구성합니다:

- **MockVarexSDK:** Varex SDK API의 인터페이스를 모방, 콜백 시뮬레이션
- **MockVieworksSDK:** Vieworks SDK 폴링 API를 모방, 상태 반환 시뮬레이션
- **MockABYZSDK:** ABYZ SDK 기본 API 모방

### 조건부 빌드 전략

CMake에서 벤더 SDK 존재 여부를 확인하여:
- SDK가 설치된 경우: 실제 SDK 링크 빌드
- SDK가 미설치된 경우: 해당 어댑터 빌드 스킵 (WARNING 출력)
- Mock SDK: 단위 테스트에서는 항상 Mock SDK 링크

---

## 3. 파일 규모

| 범주 | 파일 수 | 예상 라인 수 |
|------|---------|-------------|
| Varex 어댑터 | 4 | ~385L |
| Vieworks 어댑터 | 4 | ~430L |
| ABYZ 어댑터 | 4 | ~350L |
| 공통 인프라 (CMake) | 1 | ~80L |
| **합계** | **13** | **~1,245L** |

---

## 4. 위험 대응 계획

### 위험 1: SDK 미확보

- **대응:** Mock SDK 레이어로 개발 진행, 실제 SDK 확보 후 통합 테스트
- **영향:** ABYZAdapter가 스켈레톤 상태로 남을 수 있음

### 위험 2: SDK API 가변성

- **대응:** EmulAdapter의 공통 패턴을 기반으로 하되, 벤더별 특화 로직을 유연하게 수용
- **영향:** 예상보다 많은 벤더별 특화 코드가 필요할 수 있음

### 위험 3: 폴링 기반 SDK 성능

- **대응:** 폴링 주기를 설정 가능하게 하고, 폴링 스레드의 CPU 사용량 모니터링
- **영향:** VieworksAdapter의 응답 시간이 콜백 기반 대비 높을 수 있음

---

## 5. 추적성 태그

- [SPEC-ADAPTER-002/plan] -> [SPEC-ADAPTER-002/spec]: EARS 요구사항 구현 계획
- [SPEC-ADAPTER-002/plan] -> [SPEC-ADAPTER-001]: EmulAdapter 패턴 재사용
- [SPEC-ADAPTER-002/plan] -> [SPEC-INTEG-001]: 통합 테스트 대상 제공
