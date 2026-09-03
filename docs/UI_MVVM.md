# UI MVVM 구조

## 목표

인게임 HUD가 게임 객체를 직접 조회하지 않고 ViewModel의 `FieldNotify` 값을 바인딩하도록 구성한다. ViewModel이 늘어나도 `ARSPlayerController`가 구체적인 ViewModel 클래스와 초기화 함수를 하나씩 알지 않게 한다.

현재 범위는 싱글 플레이이며 네트워크 복제는 고려하지 않는다.

## 책임

### Widget과 HUD

- Widget Blueprint는 필요한 ViewModel을 Manual Source로 선언하고 표시 값만 바인딩한다.
- `ARSPlayerHeadUpDisplay`는 루트 Widget의 Source 클래스를 읽어 `URSLocalPlayerViewModelSubsystem`에서 같은 클래스의 ViewModel을 생성하거나 조회한 뒤 Widget에 설정한다.
- Widget은 게임 객체의 탐색과 이벤트 구독을 담당하지 않는다.

### LocalPlayer ViewModel Subsystem

- LocalPlayer가 공유하는 ViewModel을 클래스별로 하나씩 생성하고 보관한다.
- 현재 게임 데이터 원본을 약한 참조로 보관한다.
- 데이터 원본이 먼저 등록되면 기존 ViewModel에 즉시 전달한다.
- ViewModel이 나중에 생성되면 보관한 데이터 원본을 다시 전달한다.
- 구체적인 ViewModel 타입이나 각 데이터 원본의 사용 목적은 판단하지 않는다.

### LocalPlayer ViewModel

- `URSLocalPlayerViewModelBase`는 데이터 원본 등록과 해제 통지만 정의한다.
- 각 ViewModel은 전달된 `UObject` 중 자신이 지원하는 타입만 선택한다.
- 선택한 객체의 Component와 이벤트를 구독하여 Widget에 제공할 값으로 변환한다.
- 데이터 원본을 강하게 소유하지 않고, 교체·해제·파괴 시 등록한 이벤트를 정리한다.
- 공통 기반에는 체력과 같은 도메인 데이터를 두지 않는다.

### 게임 객체

- 게임 객체는 ViewModel 클래스를 직접 참조하지 않는다.
- 로컬 UI에서 관찰할 객체가 활성화되거나 해제될 때 자신을 범용 데이터 원본으로 등록하거나 해제한다.
- `ARSPlayerController`는 자신의 Pawn을 등록하며, `ARSBossEncounter`는 현재 보스를 참가자의 범용 등록 경로로 전달한다.

## 데이터 흐름

```text
게임 객체
  -> RegisterSource / UnregisterSource
  -> URSLocalPlayerViewModelSubsystem
  -> URSLocalPlayerViewModelBase
  -> 구체 ViewModel이 지원하는 타입만 선택
  -> FieldNotify 값 변경
  -> Widget Binding 갱신
```

현재 Player와 Boss 체력 흐름은 다음과 같다.

```text
ARSPlayerCharacter -> URSPlayerStatusViewModel -> Player Health Widget
ARSBossCharacter   -> URSBossStatusViewModel   -> Boss Health Widget
```

두 ViewModel의 체력 관찰 구현은 현재 독립적으로 유지한다. 실제 공통 요구가 체력 외에도 확인되기 전에는 공통 체력 ViewModel을 만들지 않는다.

## ViewModel 추가 기준

1. `URSLocalPlayerViewModelBase`를 상속한 ViewModel을 만든다.
2. `HandleSourceRegistered`에서 지원하는 게임 객체 타입만 선택한다.
3. `HandleSourceUnregistered`에서는 현재 연결한 원본과 일치할 때만 해제한다.
4. Widget Blueprint에 ViewModel을 Manual Source로 추가한다.
5. 원본을 소유한 게임 흐름에서 `RegisterSource`와 `UnregisterSource`를 대칭적으로 호출한다.

`ARSPlayerController`에 ViewModel 클래스별 생성, 조회, 초기화 또는 해제 함수를 추가하지 않는다.

## 적용 범위와 한계

- Subsystem은 ViewModel 클래스별 인스턴스를 하나만 보관하므로 현재 플레이어, 현재 보스처럼 LocalPlayer HUD가 공유하는 단일 상태에 사용한다.
- 인벤토리 행처럼 같은 ViewModel 타입이 동시에 여러 인스턴스 필요한 경우 Widget이 행별 ViewModel을 소유한다.
- `UObject` 기반 Source 전달은 결합을 줄이지만 지원 타입이 함수 구현 안에 있으므로 컴파일 시점 계약이 약하다.
- 같은 종류의 Source가 동시에 여러 개 등록되면 선택 우선순위가 정의되어 있지 않다. 다중 보스 등 실제 요구가 생기면 식별자나 명시적인 선택 정책을 추가한다.
- Source를 등록한 객체는 종료 경로에서도 반드시 같은 객체를 해제해야 한다. 비정상적인 생명주기 경로는 PIE에서 확인한다.
