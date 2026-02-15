# UXDI 프로젝트 제품 정의서

## 프로젝트 개요

**프로젝트명:** UXDI (Universal X-ray Detector Interface)

**분류:** 의료 영상 기기 통합 미들웨어

**상태:** 활발한 개발 중

---

## 프로젝트 설명

UXDI는 다양한 X-ray Detector 제조사의 SDK를 단일 표준 인터페이스(IDetector)로 추상화하는 미들웨어입니다.

현재 X-ray 의료 영상 콘솔은 여러 벤더(Varex, Vieworks, Rayence, Samsung, DRTech, ABYZ)의 디텍터를 지원해야 합니다. 각 벤더마다 다른 API, 통신 프로토콜, 상태 관리 방식을 사용하므로, 콘솔 개발팀은 각 벤더별로 별도의 통합 코드를 작성해야 합니다.

UXDI는 이러한 복잡성을 해결하기 위해 모든 벤더 SDK를 통일된 인터페이스 뒤에 추상화합니다. 콘솔은 UXDI의 표준 인터페이스만 사용하면 되고, 각 벤더의 어댑터는 런타임에 플러그인으로 동적 로딩됩니다.

---

## 대상 사용자

**1차 사용자: 의료 영상 콘솔 소프트웨어 개발자**

- X-ray 디텍터 통합을 담당하는 백엔드 개발자
- 디텍터로부터 수집한 이미지를 처리하는 이미지 처리 개발자
- 실시간 데이터 수집 및 분석을 구현하는 시스템 개발자

**2차 사용자: X-ray 장비 통합 엔지니어**

- 여러 벤더의 디텍터를 지원하는 콘솔 소프트웨어를 유지보수하는 엔지니어
- 하드웨어 교체 시 소프트웨어를 빠르게 적응시켜야 하는 시스템 통합 담당자
- 벤더 변경 시 기존 콘솔 코드의 수정을 최소화하려는 아키텍처 담당자

---

## 핵심 목표

### 1. Vendor Agnosticism (제조사 종속성 완전 제거)

**문제:** 현재 콘솔은 각 벤더의 SDK에 종속되어 있어, 벤더 변경 시 콘솔 코드를 대대적으로 수정해야 합니다.

**솔루션:** Interface-based Design을 통해 콘솔은 추상 인터페이스에만 의존합니다.

- IDetector 추상 인터페이스: 모든 벤더 어댑터가 구현해야 하는 순수 가상 인터페이스
- 구체적인 벤더 SDK는 어댑터 뒤에 캡슐화
- 런타임에 필요한 어댑터만 동적 로딩
- 벤더 변경 시 어댑터만 교체, 콘솔 코드 무수정

**기대 효과:** 재컴파일 및 콘솔 코드 수정 없이 플러그인 교체만으로 새 벤더 지원 가능

### 2. Zero-Copy Pipeline (메모리 효율성)

**문제:** X-ray 디텍터는 16-bit 대용량 RAW 이미지(예: 2048x2048 해상도 = 8MB)를 초당 수십 회 생성합니다. 각 단계에서 복사가 발생하면 메모리 대역폭이 병목이 되고, 처리 지연이 증가합니다.

**솔루션:** shared_ptr 기반의 Zero-Copy 메모리 관리

- 이미지 프레임을 동적으로 할당하고 shared_ptr로 참조
- 벤더 어댑터에서 콘솔의 처리 단계로 프레임을 전달할 때, 포인터 전달(복사 X)
- 모든 처리 단계가 동일한 메모리 버퍼 참조
- 마지막 사용자가 해제될 때만 메모리 반환

**기대 효과:** 메모리 대역폭 50~80% 감소, 처리 레이턴시 30~40% 개선

### 3. Reliability (안정성 및 격리)

**문제:** 특정 벤더의 SDK 버그나 크래시가 발생하면, 전체 콘솔이 영향을 받습니다. 케이블 분리 같은 하드웨어 오류는 예측 불가능한 예외를 발생시킵니다.

**솔루션:** 격리 및 자동 복구

- 각 벤더 어댑터의 예외는 IDetectorListener를 통해 콘솔에 안전하게 전달
- 상태 머신 기반의 상태 추적으로 일관된 상태 유지
- SDK 크래시 감지 및 안전한 shutdown
- 케이블 분리 등의 하드웨어 오류는 DetectorState.Error로 전환
- 콘솔은 하드웨어 오류에 우아하게 대응 가능

**기대 효과:** 시스템 안정성 향상, MTBF(평균 무고장 시간) 증가

---

## 핵심 기능

### 1. IDetector 추상 인터페이스

UXDI의 핵심 인터페이스로, 모든 벤더 어댑터가 구현해야 합니다.

**주요 메서드:**

- `Connect()`: 디텍터에 연결
- `Disconnect()`: 디텍터 연결 해제
- `GetState()`: 현재 상태 반환
- `Prepare()`: 촬영 준비 (gain/offset 설정)
- `Fire()`: X-ray 촬영 트리거
- `Acquire()`: 이미지 데이터 수집
- `GetFrame()`: shared_ptr로 최신 프레임 반환

**특징:**

- Pure Virtual: 구현 없음, 모든 메서드가 순수 가상
- Exception-safe: 모든 메서드는 안전한 예외 처리
- State-aware: 현재 상태에서만 허용된 메서드 호출 가능

### 2. 벤더별 어댑터 (Adapter Pattern)

각 벤더의 SDK를 IDetector 인터페이스로 래핑합니다.

**어댑터 구조:**

- VarexDetector: Varex SDK 어댑터
- VieworksDetector: Vieworks SDK 어댑터
- RayenceDetector: Rayence SDK 어댑터
- SamsungDetector: Samsung SDK 어댑터
- DRTechDetector: DRTech SDK 어댑터
- ABYZDetector: ABYZ SDK 어댑터
- EmulDetector: 가상 디텍터 에뮬레이터 (실제 하드웨어 없이 디텍터 프로토콜 시뮬레이션)

**각 어댑터의 책임:**

- 벤더 SDK의 API 호출을 IDetector 메서드로 변환
- 벤더별 다른 상태 관리 방식을 통일된 상태 머신으로 매핑
- 벤더 SDK 예외를 안전한 IDetectorListener 콜백으로 변환
- Zero-Copy를 위해 벤더 SDK의 RAW 버퍼를 shared_ptr로 래핑

### 3. DLL Plugin Factory (런타임 동적 로딩)

어댑터를 런타임에 동적으로 로딩하는 메커니즘입니다.

**기능:**

- 각 벤더 어댑터는 별도 DLL(또는 .so) 파일로 컴파일
- DetectorFactory는 플러그인 디렉토리에서 DLL을 검색하고 로드
- 로드한 DLL에서 CreateDetector() 팩토리 함수를 호출해 IDetector 인스턴스 생성
- 콘솔 시작 시 필요한 어댑터만 로드

**장점:**

- 새 벤더 지원을 위해 콘솔을 재컴파일할 필요 없음
- 사용하지 않는 어댑터를 메모리에 로드하지 않음
- 빠른 벤더 교체 및 프로토타이핑

### 4. Observer Pattern 기반 이벤트 시스템 (IDetectorListener)

디텍터의 상태 변화와 이미지 도착을 콘솔에 알립니다.

**주요 콜백:**

- `OnStateChanged(DetectorState new_state)`: 상태 변화 알림 (예: Idle → ReadyToFire)
- `OnFrameAcquired(shared_ptr<Frame> frame)`: 새 이미지 프레임 도착 알림
- `OnError(ErrorCode code, string message)`: 오류 발생 알림 (예: 케이블 분리)
- `OnTemperatureWarning()`: 온도 경고 알림

**특징:**

- 멀티 리스너: 여러 콘솔 컴포넌트가 동시에 이벤트 구독 가능
- 비동기 콜백: 어댑터는 콜백을 별도 스레드에서 호출 가능
- 타입 안전: C++ 함수 객체(functor) 또는 람다 사용

### 5. 상태 머신 (DetectorState)

모든 디텍터의 상태를 통일된 방식으로 관리합니다.

**상태 전이 다이어그램:**

```
Disconnected → Idle → Prep → ReadyToFire → Acquiring → Processing → Idle
    ↓          ↓       ↓         ↓            ↓           ↓
  (동일)     Error   Error     Error        Error       Error
```

**상태 설명:**

- **Disconnected**: 디텍터에 연결되지 않음
- **Idle**: 대기 상태, 촬영 준비 가능
- **Prep**: 촬영 전 준비 (gain/offset 설정, 워밍업 등)
- **ReadyToFire**: 촬영 트리거 대기 중
- **Acquiring**: X-ray 촬영 진행 중, 이미지 수집 중
- **Processing**: 이미지 처리 진행 중
- **Error**: 오류 상태 (자동 복구 시도 또는 사용자 개입 필요)

**상태 머신 구현:**

- 각 상태에서 허용된 메서드만 호출 가능 (예: ReadyToFire 상태에서만 Fire() 호출 가능)
- 상태 전이는 벤더 어댑터가 제어하지만, 콘솔은 IDetectorListener를 통해 변화를 감지
- 오류 상태로의 자동 전이는 벤더 SDK의 예외를 기반으로 수행

### 6. Zero-Copy 이미지 프레임 관리 (shared_ptr)

16-bit RAW 이미지 데이터의 효율적인 관리입니다.

**Frame 구조:**

- `width`, `height`: 이미지 해상도
- `bit_depth`: 비트 깊이 (16)
- `buffer`: std::shared_ptr<uint16_t[]> - RAW 픽셀 데이터
- `timestamp`: 촬영 시간
- `metadata`: 기타 메타데이터 (gain, offset, temperature 등)

**메모리 관리:**

- 벤더 어댑터가 프레임 생성 시 shared_ptr로 메모리 할당
- 콘솔의 각 처리 단계(display, save, analysis)가 shared_ptr 복사로 참조
- 모든 처리가 완료되면 마지막 참조 해제 시 자동으로 메모리 반환
- 명시적인 delete 호출 불필요, RAII(Resource Acquisition Is Initialization) 원칙 준수

---

## 사용 사례

### 사용 사례 1: 다중 벤더 디텍터 지원

**시나리오:** 의료 영상 콘솔이 Varex와 Vieworks 디텍터를 모두 지원해야 합니다.

**기존 방식:**
- 콘솔 코드에 #ifdef로 벤더별 로직 분기
- 각 벤더 SDK의 초기화, 상태 관리, 이미지 수집 로직이 섞여 있음
- 벤더 추가 시 콘솔 코드 수정 및 재컴파일 필요

**UXDI 방식:**
- 콘솔은 IDetector 인터페이스에만 의존
- 실행 시 설정 파일에서 벤더 이름을 읽고, DetectorFactory가 해당 플러그인 로드
- 콘솔 코드는 벤더에 관계없이 동일
- 새 벤더 추가: VarexDetector/VieworksDetector/ABYZDetector.dll만 플러그인 디렉토리에 복사
- 하드웨어 없는 환경: EmulDetector.dll로 개발 및 테스트 가능

**기대 효과:**
- 콘솔 코드 간결화 (벤더 분기 로직 제거)
- 코드 유지보수성 50% 향상
- 새 벤더 추가 시간 1주일 → 1-2일

### 사용 사례 2: 하드웨어 교체 시 재컴파일 없는 플러그인 교체

**시나리오:** 병원 현장에서 Varex 디텍터를 Samsung 디텍터로 교체해야 합니다.

**기존 방식:**
- 콘솔 소스코드에서 #define DETECTOR_VENDOR를 수정
- 재컴파일 및 재테스트 (2-3시간)
- 새 실행 파일 배포

**UXDI 방식:**
- detector.ini 또는 설정 파일에서 DETECTOR_VENDOR = Samsung으로 수정
- 콘솔 재시작 시 SamsungDetector.dll 플러그인 로드
- 즉시 새 하드웨어 사용 가능

**기대 효과:**
- 다운타임 5분 이내
- 현장 대응 시간 대폭 단축
- 운영 비용 절감

### 사용 사례 3: 고해상도 X-ray 이미지의 실시간 수집 및 처리

**시나리오:** 콘솔이 2048x2048 16-bit 이미지를 초당 10회 수집하고, 동시에 화면 표시, 저장, 분석을 수행해야 합니다.

**메모리 요구사항:**
- 단일 이미지: 2048 × 2048 × 2바이트 = 8MB
- 초당 10프레임: 80MB/s 메모리 처리

**기존 방식 (복사 기반):**
- 벤더 SDK에서 이미지 수집 → 콘솔의 큐에 복사 (8MB 복사)
- 디스플레이 스레드에 복사 (8MB 복사)
- 저장 스레드에 복사 (8MB 복사)
- 분석 스레드에 복사 (8MB 복사)
- 총 메모리 대역폭: 80MB × 4 = 320MB/s

**UXDI Zero-Copy 방식:**
- 벤더 어댑터가 shared_ptr<Frame> 생성 (메모리 할당 1회)
- 각 처리 단계가 shared_ptr 복사로 참조 (포인터만 복사, 8바이트)
- 모든 처리 완료 시 자동 해제
- 총 메모리 대역폭: 80MB + 32바이트 ≈ 80MB/s

**기대 효과:**
- 메모리 대역폭 75% 감소
- CPU 캐시 미스 감소로 처리 속도 30~40% 향상
- 시스템 전체 전력 소비 감소

---

## 개발 로드맵

### Phase 1 (W1-2): Core Framework

**목표:** 핵심 프레임워크 확립 및 검증

**산출물:**
- IDetector 추상 인터페이스 확정
- DetectorState 상태 머신 구현
- IDetectorListener 콜백 인터페이스 정의
- DetectorFactory 플러그인 로더 구현
- DummyDetector (테스트용 디텍터) 구현

**활동:**
- 아키텍처 설계 및 리뷰 (1일)
- IDetector 인터페이스 정의 (2일)
- DummyDetector 구현 및 단위 테스트 (2일)
- 기본 플러그인 로더 구현 (1일)
- 통합 테스트 및 버그 수정 (1일)

**기대 산출물:**
- 코어 라이브러리 (UXDI.lib)
- DummyDetector.dll 플러그인
- 기본 문서 및 API 레퍼런스

### Phase 2 (W3-5): Pilot Implementation

**목표:** 실제 벤더 어댑터 구현 및 통합 테스트

**산출물:**
- VarexDetector 어댑터 구현 및 테스트
- VieworksDetector 어댑터 구현 및 테스트
- 통합 테스트 프레임워크 (멀티 벤더 동시 테스트)
- 성능 벤치마크 및 프로파일링

**활동:**
- Varex SDK 분석 (1일)
- VarexDetector 구현 (3일)
- Varex 어댑터 테스트 (2일)
- Vieworks SDK 분석 (1일)
- VieworksDetector 구현 (3일)
- Vieworks 어댑터 테스트 (2일)
- 통합 테스트 (2일)
- 성능 최적화 및 버그 수정 (2일)

**기대 산출물:**
- VarexDetector.dll, VieworksDetector.dll
- 멀티 벤더 통합 테스트 보고서
- Zero-Copy 성능 벤치마크 결과
- 벤더별 어댑터 개발 가이드

### Phase 3 (W6-7): Integration & Optimization

**목표:** GUI 연동, 예외 처리 강화, 최종 최적화

**산출물:**
- 샘플 GUI 애플리케이션 (Qt 또는 WinForms)
- 예외 처리 및 복구 메커니즘 강화
- 성능 최적화 (메모리, CPU, 레이턴시)
- 완전한 문서 및 예제 코드
- CI/CD 파이프라인 구축

**활동:**
- 샘플 GUI 애플리케이션 개발 (3일)
- 예외 처리 및 에러 로깅 강화 (2일)
- 성능 프로파일링 및 최적화 (2일)
- 문서 작성 및 예제 코드 준비 (2일)
- CI/CD 파이프라인 구축 (1일)
- 최종 테스트 및 버그 수정 (2일)

**기대 산출물:**
- 완성된 UXDI 프레임워크
- 샘플 GUI 애플리케이션
- 완전한 기술 문서 및 API 레퍼런스
- 벤더별 어댑터 개발 가이드 및 예제
- CI/CD 파이프라인 및 자동 테스트

**완료 기준:**
- 모든 필수 기능 구현
- 단위 테스트 커버리지 85% 이상
- 통합 테스트 통과
- 성능 벤치마크 목표 달성 (메모리 대역폭 75% 감소, 레이턴시 30~40% 개선)
- 문서 완성도 100%

---

## 기술 스택

**Core Language:** C++ 17 이상

**라이브러리:**
- Boost (smart_ptr, thread, asio)
- OpenSSL (암호화 통신)
- spdlog (로깅)

**빌드 도구:**
- CMake 3.20+
- Visual Studio 2022 (Windows)
- GCC 11+ (Linux)

**테스트 프레임워크:**
- Google Test (gtest)
- CMock (C 함수 mock)

**문서:**
- Doxygen (API 문서)
- Sphinx + ReStructuredText (사용자 문서)

---

## 성공 기준

**기능적 성공:**
- 모든 Phase의 산출물 완성
- 3개 이상 벤더 어댑터 + Emulator 모듈 구현 및 테스트 통과

**품질 기준:**
- 단위 테스트 커버리지 85% 이상
- 통합 테스트 100% 통과
- 코드 리뷰 통과 (정정식 게이트)

**성능 기준:**
- 메모리 대역폭 75% 감소 (Zero-Copy 검증)
- 이미지 처리 레이턴시 30~40% 개선
- 프레임 드롭 0 (초당 10프레임 실시간 수집)

**유지보수 기준:**
- API 문서 완성도 100%
- 벤더별 어댑터 개발 가이드 작성
- 샘플 코드 및 예제 제공

---

## 위험 요소 및 대응 방안

**위험 1: 벤더 SDK 호환성 문제**
- 각 벤더의 SDK 버전 차이로 인한 API 불일치
- 대응: Phase 1 아키텍처 설계 시 호환성 검토, 버전 매핑 계층 추가

**위험 2: Zero-Copy 구현 복잡성**
- shared_ptr 기반 메모리 관리의 복잡한 디버깅
- 대응: 철저한 단위 테스트, 메모리 프로파일링 도구 활용

**위험 3: 실시간 성능 보장**
- 멀티 벤더 동시 처리 시 CPU 경합
- 대응: Phase 2에서 성능 벤치마크 수행, 필요시 스레드 풀 최적화

**위험 4: 플러그인 로더 보안**
- 악의적 DLL 로드 가능성
- 대응: 플러그인 서명(DLL 서명) 검증, 화이트리스트 관리

---

## 참고 자료

- **아키텍처 명세:** `.moai/specs/SPEC-UXDI-ARCH/spec.md`
- **구현 가이드:** `.moai/docs/UXDI-Implementation-Guide.md`
- **API 레퍼런스:** `.moai/docs/UXDI-API-Reference.md`
- **벤더 어댑터 개발 가이드:** `.moai/docs/Adapter-Development-Guide.md`

---

**작성 일자:** 2026-02-16

**버전:** 1.0.0

**상태:** 승인 대기
