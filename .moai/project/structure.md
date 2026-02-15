# UXDI (Universal X-ray Detector Interface) 프로젝트 구조

## 1. 프로젝트 구조 개요

Universal X-ray Detector Interface(UXDI)는 플러그인 기반 아키텍처를 따르는 C++20 프로젝트입니다.
Adapter Pattern을 사용하여 서로 다른 벤더의 X-ray 디텍터 SDK를 통합할 수 있습니다.

**핵심 설계 원칙:**
- 플러그인 기반 확장성: 새로운 벤더별 어댑터를 동적으로 로드 가능
- 인터페이스 기반 설계: 공개 인터페이스를 통한 느슨한 결합
- DLL 동적 로딩: 런타임에 벤더별 어댑터를 선택적으로 로드
- ABI 안정성: 버전 호환성 보장 및 점진적 업그레이드 지원

---

## 2. 계획된 디렉토리 구조

```
sdk-wrapping-module/
├── docs/
│   └── plan.md                    # 아키텍처 명세 및 구현 가이드
│
├── include/
│   └── uxdi/
│       ├── IDetector.h            # 핵심 추상 인터페이스
│       ├── IDetectorListener.h    # 이벤트 리스너 인터페이스
│       ├── Types.h                # 공용 데이터 구조체 (ImageFrame, DetectorConfig 등)
│       └── Factory.h              # DetectorFactory 선언
│
├── src/
│   ├── core/
│   │   ├── DetectorFactory.cpp    # DLL 동적 로더 구현
│   │   └── DetectorManager.cpp    # 디텍터 생명주기 관리
│   │
│   └── adapters/
│       ├── dummy/
│       │   └── DummyAdapter.cpp   # 시뮬레이터 어댑터 (하드웨어 없이 개발용)
│       │
│       ├── emul/
│       │   └── EmulAdapter.cpp    # 가상 디텍터 에뮬레이터 (프로토콜 시뮬레이션)
│       │
│       ├── varex/
│       │   └── VarexAdapter.cpp   # Varex SDK 래핑 어댑터
│       │
│       ├── vieworks/
│       │   └── VieworksAdapter.cpp # Vieworks SDK 래핑 어댑터
│       │
│       └── abyz/
│           └── ABYZAdapter.cpp    # ABYZ SDK 래핑 어댑터
│
├── tests/
│   ├── unit/
│   │   ├── test_factory.cpp       # Factory 단위 테스트
│   │   └── test_dummy_adapter.cpp # Dummy 어댑터 테스트
│   │
│   └── integration/
│       └── test_adapter_lifecycle.cpp # 어댑터 생명주기 통합 테스트
│
├── cmake/
│   └── FindVendorSDK.cmake        # 벤더 SDK 검색 모듈
│
├── CMakeLists.txt                 # 빌드 설정
├── .gitignore
└── README.md
```

---

## 3. 핵심 디렉토리 설명

### 3.1 include/uxdi/ - 공개 헤더 파일

**목적:** UXDI 프레임워크의 공개 인터페이스 정의

**파일 구성:**
- `IDetector.h`: 모든 어댑터가 구현해야 하는 핵심 인터페이스
  - 이미지 획득 (Acquire)
  - 설정 관리 (GetConfig, SetConfig)
  - 상태 조회 (IsConnected, GetFrameRate)

- `IDetectorListener.h`: 비동기 이벤트 콜백 인터페이스
  - 이미지 수신 이벤트 (OnFrameAcquired)
  - 에러 이벤트 (OnError)
  - 상태 변경 이벤트 (OnStatusChanged)

- `Types.h`: 공용 데이터 구조체
  - `ImageFrame`: 획득한 이미지 데이터 + 메타데이터
  - `DetectorConfig`: 디텍터 설정 정보
  - `DetectorInfo`: 디텍터 정보 및 사양
  - 상수 및 열거형 정의

- `Factory.h`: DetectorFactory 선언
  - 어댑터 검색 및 로드 함수
  - 디텍터 생성 함수

**ABI 안정성:** 모든 공개 인터페이스는 virtual 메서드로 정의되어 버전 간 호환성 보장

### 3.2 src/core/ - 핵심 프레임워크

**목적:** 플러그인 로딩 및 생명주기 관리

**파일 구성:**
- `DetectorFactory.cpp`: DLL 동적 로더 구현
  - 플랫폼별 동적 로딩 (Windows: LoadLibrary, Linux: dlopen)
  - 심볼 해석 및 함수 포인터 획득
  - 버전 호환성 검증

- `DetectorManager.cpp`: 디텍터 생명주기 관리
  - 디텍터 인스턴스 생성 및 소멸
  - 멀티 디텍터 지원
  - 리소스 정리 및 메모리 누수 방지

### 3.3 src/adapters/ - 벤더별 어댑터 구현

**목적:** 각 벤더 SDK를 UXDI 인터페이스로 래핑

**특징:**
- 각 어댑터는 독립적인 DLL로 빌드
- 어댑터별 의존성 격리 (벤더 SDK는 DLL 내부에만 포함)
- IDetector 인터페이스 구현 필수

**어댑터 종류:**

1. **dummy/** - 시뮬레이터 어댑터
   - 하드웨어 없이 개발 및 테스트 가능
   - 더미 이미지 데이터 생성
   - 완전히 독립적 (외부 의존성 없음)

2. **varex/** - Varex SDK 래핑
   - Varex XRD 디텍터 지원
   - Varex SDK에 대한 종속성 격리
   - 하드웨어 특화 기능 구현

3. **vieworks/** - Vieworks SDK 래핑
   - Vieworks 카메라 기반 디텍터 지원
   - Vieworks SDK에 대한 종속성 격리
   - 하드웨어 특화 기능 구현

4. **abyz/** - ABYZ SDK 래핑
   - ABYZ 디텍터 지원
   - ABYZ SDK에 대한 종속성 격리
   - 하드웨어 특화 기능 구현

5. **emul/** - 가상 디텍터 에뮬레이터
   - 실제 디텍터 프로토콜 에뮬레이션 (Dummy보다 고급)
   - 다양한 시나리오 시뮬레이션 (정상 촬영, 오류 발생, 지연, 재연결)
   - 외부 의존성 없이 독립 실행 가능
   - CI/CD 파이프라인 및 자동화 테스트에 활용

### 3.4 tests/ - 테스트 스위트

**목적:** 단위 테스트 및 통합 테스트

**테스트 구조:**
- `unit/`: 개별 컴포넌트 단위 테스트
  - Factory 로딩 및 DLL 해석 테스트
  - Dummy 어댑터 정상 작동 테스트

- `integration/`: 전체 시스템 통합 테스트
  - 어댑터 생명주기 (생성 → 초기화 → 정상작동 → 정리) 테스트
  - 멀티 디텍터 동시 작동 테스트
  - 에러 핸들링 및 복구 테스트

**테스트 프레임워크:** Google Test (gtest)

### 3.5 cmake/ - 빌드 설정

**목적:** CMake 빌드 시스템 지원

**파일:**
- `FindVendorSDK.cmake`: 벤더 SDK 자동 검색
  - Varex SDK 위치 검색
  - Vieworks SDK 위치 검색
  - ABYZ SDK 위치 검색
  - 설치되지 않은 SDK는 빌드에서 자동 제외
  - Emulator는 외부 SDK 불필요 (항상 빌드 가능)

---

## 4. 모듈 구성

### 4.1 UXDI Core Layer

**구성:** 인터페이스 정의 (include/uxdi/) + 핵심 구현 (src/core/)

**형태:** 정적 라이브러리 또는 헤더 전용 (헤더 전용 권장)

**책임:**
- IDetector 인터페이스 정의
- Factory를 통한 어댑터 동적 로딩
- 기본 생명주기 관리

**특징:**
- 벤더 SDK에 대한 의존성 없음
- 모든 어댑터의 기반이 되는 계약(Contract) 정의

### 4.2 Vendor Adapter Layer

**구성:** 각 벤더별 DLL (src/adapters/[vendor]/)

**특징:**
- 독립적 빌드: 각 어댑터는 별도 DLL로 컴파일
- 캡슐화: 벤더 SDK는 DLL 내부에만 포함
- 선택적 로딩: 필요한 어댑터만 런타임에 로드
- 런타임 플러그인: 새 버전 추가 시 메인 애플리케이션 수정 불필요

**어댑터별 책임:**
- IDetector 인터페이스 100% 구현
- 벤더 SDK와의 상호 작용
- 에러 처리 및 예외 관리
- 리소스 정리 (메모리, 스레드 등)

### 4.3 Test Suite

**구성:** Google Test 기반 단위/통합 테스트

**목표:**
- 각 어댑터의 IDetector 구현 검증
- Factory 및 Manager의 정상 작동 확인
- 멀티 디텍터 시나리오 테스트
- 에러 복구 능력 검증

**커버리지:** 85% 이상

---

## 5. 파일 소유권 패턴 (개발 시 참조)

### 5.1 공개 인터페이스 파일

**파일:** `include/uxdi/*.h`

**특징:**
- 모든 어댑터가 구현해야 하는 계약
- 변경 시 모든 어댑터에 영향

**변경 정책:**
- 신중한 검토 필요
- 하위 호환성 보장 (기존 메서드 제거 금지)
- 새 기능은 새 메서드로 추가
- 변경 전 모든 어댑터 영향도 분석

### 5.2 어댑터별 구현 파일

**파일:** `src/adapters/[vendor]/*`

**특징:**
- 각 어댑터는 완전히 독립적
- 다른 어댑터와 코드 공유 불가
- 벤더 SDK 의존성은 DLL 내부에만 국한

**개발 정책:**
- 어댑터별 독립적 개발 가능
- 다른 어댑터에 미치는 영향 없음
- 어댑터 내부 구현은 자유도가 높음

### 5.3 핵심 구현 파일

**파일:** `src/core/`

**특징:**
- Factory와 Manager는 모든 어댑터의 기반
- 변경 시 모든 어댑터에 영향 가능

**변경 정책:**
- 어댑터 호환성 영향도 분석 필수
- 새 기능 추가 시 기존 동작 보존
- 변경 후 모든 어댑터 테스트 필수

### 5.4 테스트 파일

**파일:** `tests/`

**특징:**
- 각 어댑터별 독립 테스트: `tests/unit/test_[adapter].cpp`
- 공통 통합 테스트: `tests/integration/`

**개발 정책:**
- 어댑터 변경 시 어댑터별 단위 테스트 필수
- 공개 인터페이스 변경 시 통합 테스트 갱신
- 커버리지 85% 이상 유지

---

## 6. 빌드 및 배포 전략

### 6.1 정적/동적 링크 선택

**UXDI Core:**
- 옵션 1: 정적 라이브러리 (권장) - 배포 간단
- 옵션 2: 헤더 전용 - 컴파일 타임 오버헤드 없음

**Vendor Adapters:**
- DLL (동적 라이브러리) - 런타임 플러그인

### 6.2 빌드 구성

**CMakeLists.txt 계층:**
- 최상위: 전체 프로젝트 설정
- src/core: UXDI Core 빌드
- src/adapters/[vendor]: 각 어댑터 빌드
- tests: 테스트 빌드

**조건부 빌드:**
```
IF Varex SDK 설치됨:
  어댑터 빌드 활성화
ELSE:
  어댑터 빌드 비활성화 (경고 출력)
```

### 6.3 배포 구조

```
배포/
├── headers/
│   └── uxdi/                      # 공개 헤더 (개발자용)
├── lib/
│   ├── uxdi_core.lib              # UXDI Core (정적/동적)
│   ├── adapters/
│   │   ├── uxdi_dummy.dll
│   │   ├── uxdi_emul.dll
│   │   ├── uxdi_varex.dll
│   │   ├── uxdi_vieworks.dll
│   │   └── uxdi_abyz.dll
└── docs/
    └── API 문서
```

---

## 7. 개발 워크플로우

### 7.1 새 어댑터 추가

1. `src/adapters/[new_vendor]/` 디렉토리 생성
2. `[new_vendor]Adapter.cpp` 구현 (IDetector 인터페이스)
3. `CMakeLists.txt` 추가 및 SDK 검색 로직
4. `tests/unit/test_[new_vendor].cpp` 테스트 작성
5. `tests/integration/` 테스트에 통합 테스트 추가
6. 모든 테스트 통과 확인 (85% 커버리지)

### 7.2 공개 인터페이스 변경

1. 변경 전 영향도 분석 (모든 어댑터 검토)
2. `include/uxdi/` 인터페이스 수정
3. 모든 어댑터 구현 업데이트
4. 모든 테스트 케이스 업데이트
5. 통합 테스트 전체 통과 확인

### 7.3 버전 관리

- 주버전(Major): 주요 인터페이스 변경
- 부버전(Minor): 기능 추가 (하위 호환성 유지)
- 패치(Patch): 버그 수정

---

## 8. 의존성 관리

### 8.1 외부 의존성

**UXDI Core:**
- C++ 표준 라이브러리만 사용

**Vendor Adapters:**
- 벤더별 SDK (DLL 내부에만 포함)

**Tests:**
- Google Test (gtest)
- CMake 3.15 이상

### 8.2 플랫폼 지원

- **Windows:** MSVC 2019+, Visual Studio 16+
- **Linux:** GCC 9+, Clang 10+
- **macOS:** Clang 12+, Xcode 12+

---

## 9. 문서 구조

**docs/plan.md:**
- 아키텍처 명세
- 구현 가이드
- API 레퍼런스
- 어댑터 개발 가이드

**README.md:**
- 프로젝트 개요
- 빌드 방법
- 설치 및 사용 방법
- 예제 코드

**코드 주석:**
- Doxygen 형식 주석
- 공개 인터페이스 상세 설명
- 복잡한 알고리즘 설명

---

## 10. 품질 보증 기준

### 10.1 코드 품질

- **커버리지:** 85% 이상
- **린팅:** 모든 경고 제거
- **코드 스타일:** Google C++ Style Guide 준수

### 10.2 테스트 요구사항

- 모든 공개 인터페이스 단위 테스트 필수
- 어댑터별 통합 테스트 필수
- 멀티 디텍터 시나리오 테스트
- 에러 처리 및 복구 테스트

### 10.3 문서화

- 모든 공개 API 주석 필수
- 어댑터 개발 가이드 제공
- 예제 코드 포함

---

**최종 수정일:** 2026-02-16
**프로젝트 상태:** 설계 단계
**다음 단계:** SPEC 문서 작성 및 Dummy 어댑터 구현
