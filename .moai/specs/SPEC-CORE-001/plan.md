# SPEC-CORE-001: 구현 계획

## 메타데이터

| 항목 | 내용 |
|------|------|
| **SPEC ID** | SPEC-CORE-001 |
| **제목** | UXDI Core Framework - 구현 계획 |
| **개발 모드** | Hybrid (TDD - 모든 코드가 신규) |
| **예상 파일 수** | 9개 프로덕션 파일 |
| **예상 코드 라인** | ~600L |

---

## 1. 구현 단계 (우선순위 기반)

### Primary Goal: 헤더 파일 정의

**우선순위**: Critical - 모든 후속 구현의 기반

**대상 파일:**
1. `include/uxdi/Export.h` - DLL Export/Import 매크로 정의
2. `include/uxdi/Types.h` - 공용 타입, enum, 구조체 정의
3. `include/uxdi/IDetectorListener.h` - 리스너 콜백 인터페이스
4. `include/uxdi/IDetector.h` - 핵심 추상 인터페이스
5. `include/uxdi/Factory.h` - DetectorFactory 클래스 선언

**TDD 접근:**
- RED: 각 헤더의 컴파일 테스트 작성 (include 가능 여부, 타입 크기 검증)
- GREEN: 헤더 파일 구현
- REFACTOR: 코드 스타일 정리, Doxygen 주석 추가

**완료 기준:**
- 모든 헤더가 독립적으로 컴파일 가능
- Include guard 적용
- ABI 안정성 규칙 준수 (멤버 변수 없음)

---

### Secondary Goal: DetectorFactory 구현

**우선순위**: High - 플러그인 로딩 핵심 기능

**대상 파일:**
1. `src/core/DetectorFactory.cpp` - DLL 로딩 및 인스턴스 생성

**TDD 접근:**
- RED: DLL 로딩 실패 테스트, 심볼 해석 실패 테스트, 성공 로딩 테스트
- GREEN: LoadLibraryA/GetProcAddress 기반 구현
- REFACTOR: 에러 메시지 개선, 리소스 정리 보장

**테스트 시나리오:**
- 존재하지 않는 DLL 경로 -> `std::runtime_error` throw
- 유효한 DLL이지만 CreateDetector 심볼 없음 -> `std::runtime_error` throw
- 유효한 DLL이지만 DestroyDetector 심볼 없음 -> `std::runtime_error` throw
- 유효한 어댑터 DLL 로드 -> `shared_ptr<IDetector>` 반환

**완료 기준:**
- DLL 로드 성공/실패 시나리오 모두 테스트 통과
- shared_ptr custom deleter를 통한 안전한 메모리 해제
- DLL 핸들 누수 없음

---

### Tertiary Goal: DetectorManager 구현

**우선순위**: High - 생명주기 관리

**대상 파일:**
1. `src/core/DetectorManager.cpp` - 디텍터 등록/조회/종료

**TDD 접근:**
- RED: 등록/조회/해제/전체 종료 테스트
- GREEN: `std::unordered_map` 기반 인스턴스 관리 구현
- REFACTOR: 스레드 안전성 추가 (mutex)

**테스트 시나리오:**
- RegisterDetector -> GetDetector로 조회 성공
- 중복 이름 등록 시 실패 또는 덮어쓰기 정책
- UnregisterDetector 후 GetDetector -> nullptr 반환
- ShutdownAll -> 모든 디텍터 Disconnect 호출 및 정리

**완료 기준:**
- 전체 생명주기(등록-조회-해제-종료) 테스트 통과
- 스레드 안전성 보장 (mutex 보호)
- 메모리 누수 없음

---

### Final Goal: 빌드 시스템 구성

**우선순위**: Medium - 빌드 인프라

**대상 파일:**
1. `CMakeLists.txt` (루트) - 프로젝트 설정, C++20 표준, GTest 연동
2. `src/CMakeLists.txt` - uxdi_core 정적 라이브러리 타겟

**구현 내용:**
- CMake 최소 버전 3.20 설정
- C++20 표준 활성화 (`cxx_std_20`)
- uxdi_core 정적 라이브러리 타겟 정의
- include 디렉토리 설정 (PUBLIC)
- GTest 의존성 추가 (FetchContent 또는 find_package)
- CTest 통합

**완료 기준:**
- `cmake --build .` 성공
- `ctest` 실행 시 모든 테스트 통과
- uxdi_core.lib 생성 확인

---

## 2. 기술적 접근 방식

### TDD 전략 (신규 프로젝트)

이 프로젝트는 Greenfield이므로 모든 코드에 TDD(RED-GREEN-REFACTOR) 사이클을 적용합니다.

**테스트 우선순위:**
1. 단위 테스트: Factory DLL 로딩, Manager 생명주기, 상태 전이 검증
2. 컴파일 테스트: 헤더 독립 컴파일, ABI 규칙 준수 확인
3. 통합 테스트: Factory + Manager + DummyAdapter 연동 (SPEC-ADAPTER-001 완료 후)

### 아키텍처 설계 방향

**정적 라이브러리 (uxdi_core):**
- 공개 헤더: `include/uxdi/` 디렉토리
- 구현 파일: `src/core/` 디렉토리
- 어댑터 DLL이 이 라이브러리에 링크

**DLL 플러그인 시스템:**
- 각 어댑터는 독립 DLL
- Factory가 런타임에 DLL 로드
- extern "C" 함수를 통한 인스턴스 생성/소멸

### 코드 스타일

- Google C++ Style Guide 준수
- Doxygen 주석 형식
- Include guard 사용 (`#pragma once`)
- 네임스페이스: `uxdi::`

---

## 3. 테스트 계획

### 단위 테스트 파일 구조

```
tests/
├── unit/
│   ├── test_types.cpp            - Types.h 타입 검증
│   ├── test_factory.cpp          - DetectorFactory DLL 로딩 테스트
│   ├── test_manager.cpp          - DetectorManager 생명주기 테스트
│   └── test_state_machine.cpp    - 상태 전이 규칙 테스트
```

### 테스트 커버리지 목표

- Factory: DLL 로딩 성공/실패, 심볼 해석 성공/실패, custom deleter 검증
- Manager: 등록/조회/해제/전체종료, 중복 등록, 빈 Manager 조회
- 상태 머신: 각 상태에서 허용/금지 메서드 호출, 상태 전이 알림

---

## 4. 위험 관리

| 위험 | 완화 전략 |
|------|----------|
| ABI/vtable 불일치 | 동일 MSVC 버전 빌드 강제, CI에서 검증 |
| FreeLibrary 순서 문제 | shared_ptr에 DLL 핸들 포함, custom deleter에서 순서 보장 |
| DLL 로딩 스레드 안전성 | Factory에 std::mutex 적용 |

---

## 5. 후속 단계

- SPEC-ADAPTER-001과의 통합 테스트 (DummyAdapter를 통한 Factory/Manager 검증)
- 실제 벤더 어댑터 SPEC (Phase 2)에서 이 Core Framework 활용

---

## 추적성 태그

- **SPEC 참조**: SPEC-CORE-001/spec.md
- **수락 기준**: SPEC-CORE-001/acceptance.md
- **프로젝트 Phase**: Phase 1 (W1-2)
