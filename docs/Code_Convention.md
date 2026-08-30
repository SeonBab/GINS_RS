# Code Convention

## 줄바꿈

메서드 체이닝 호출은 중간에 줄바꿈하지 않고 한 줄로 작성한다.

```cpp
// Bad
AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(
    URSHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);

// Good
AbilitySystemComp->GetGameplayAttributeValueChangeDelegate(URSHealthSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedDelegateHandle);
```

## 주석

### 기본 원칙

- 주석은 한국어로 작성하고 클래스명, 함수명, 타입명, 시스템 명칭 등의 고유 식별자는 코드의 표기를 유지한다
- 문장은 `~합니다` 형식으로 작성하고 문장 끝의 마침표는 생략한다
- 주석은 코드를 한 줄씩 해석하거나 동작을 그대로 설명하지 않는다
- 코드가 왜 필요한지, 어떤 의도로 작성되었는지, 어떤 제약과 예외가 있는지처럼 코드 자체에 드러나지 않는 정보를 설명한다
- 주석은 설명하는 대상의 바로 위에 작성하며 대상과 같은 수준으로 들여쓴다
- 코드가 변경되면 관련 주석도 함께 수정하여 실제 동작과 일치하게 유지한다

```cpp
// Bad: 코드를 그대로 읽어 설명합니다
// 최대 이동 속도에 SprintSpeed를 대입합니다
MovementComponent->MaxWalkSpeed = SprintSpeed;

// Good: 이 코드를 작성한 의도를 설명합니다
// 종료 시 정확한 값으로 되돌릴 수 있도록 적용 직전의 이동 속도를 보관합니다
PreviousMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
```

### 선언부 주석

헤더 파일에서 클래스, 구조체, 열거형, 함수, 멤버 변수의 역할을 설명할 때는 문서 주석 `/** ... */`을 사용한다.

한 문장으로 충분한 설명은 한 줄로 작성한다.

```cpp
/** 현재 체력을 반환합니다 */
float GetHealth() const;
```

두 문장 이상이 필요하면 여러 줄로 작성하고 각 줄 앞에 ` *`를 붙인다.

```cpp
/**
 * HealthSet의 체력 변경을 캐릭터, UI 등의 외부 시스템에 전달합니다
 * 실제 체력 데이터는 소유하지 않고 ASC와 HealthSet을 연결하는 역할을 합니다
 */
UCLASS()
class RS_API URSHealthComponent : public UActorComponent
```

### 구현부 주석

함수 내부의 처리 의도, 분기 이유, 예외 상황, 상태 변화 등을 설명할 때는 한 줄 주석 `//`을 사용한다.

```cpp
// 입력 조건이 중간에 취소되어도 ASC의 Held 상태가 남지 않도록 해제 콜백을 호출합니다
BindAction(Action.InputAction, ETriggerEvent::Canceled, Object, ReleasedFunc, Action.InputTag);
```

연속된 주석이 하나의 설명이라면 각 줄에 `//`을 사용한다.

```cpp
// Pressed와 Released는 한 프레임 상태이므로 처리 후 초기화합니다
// Held는 입력을 해제할 때까지 다음 프레임에도 유지합니다
InputPressedSpecHandles.Reset();
InputReleasedSpecHandles.Reset();
```

헤더의 문서 주석과 같은 내용을 소스 파일의 함수 정의 위에 반복하지 않는다. 구현에서 추가로 알아야 할 의도가 있을 때만 함수 내부에 주석을 작성한다.

### 그룹 주석

Gameplay Tag 선언처럼 같은 성격의 항목을 묶을 때는 짧은 영문 명사 형태의 한 줄 주석을 사용한다.

```cpp
// Input
RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);

// Ability
RS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Sprint);
```

### 주석 처리된 코드

사용하지 않는 코드를 주석으로 남겨두지 않는다. 다시 필요할 수 있는 코드는 버전 관리 기록을 통해 확인한다.

```cpp
// Bad
// const FRotator CameraRotation = CameraComp->GetComponentRotation();
// const FRotator YawRotation(0.0f, CameraRotation.Yaw, 0.0f);
```

## 클래스 구성

### 기본 구성 순서

클래스는 다음 순서로 구성한다.

1. `GENERATED_BODY()`
2. 생성자
3. 여러 기능에 걸친 Unreal 생명주기 함수
4. 기능별 `#pragma region`
5. region으로 구분하지 않는 멤버

생성자와 Unreal 생명주기 함수는 클래스 상단에 작성한다. 특정 기능에만 속하는 Unreal 오버라이드 함수는 해당 기능의 region에 작성할 수 있다.

```cpp
UCLASS()
class RS_API ARSPlayerCharacter : public ARSBaseCharacter
{
	GENERATED_BODY()

public:
	ARSPlayerCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// 기능별 region
};
```

### 함수와 변수

함수와 멤버 변수는 별도의 그룹으로 구분하며 함수를 먼저 작성한다. 각 그룹은 접근 지정자를 독립적으로 적용하고 `public`, `protected`, `private` 순서로 작성한다.

```text
public 함수
protected 함수
private 함수

public 변수
protected 변수
private 변수
```

- 함수와 변수의 접근 수준이 같아도 그룹이 바뀌면 접근 지정자를 다시 작성한다
- 해당하는 멤버가 없는 접근 지정자는 생략한다
- 서로 다른 그룹 사이에는 빈 줄을 둔다
- 외부에서 직접 사용할 필요가 없다면 가장 제한적인 접근 수준을 사용한다

### `#pragma region`

`#pragma region`은 관련된 함수와 변수가 충분히 많아 기능 단위 구분이 코드 탐색에 도움이 될 때 사용한다.

- `Input`, `GAS`, `Health`처럼 기능 또는 시스템 단위의 이름을 사용한다
- `Public`, `Functions`, `Variables`처럼 선언 형태만 나타내는 이름은 사용하지 않는다
- 멤버가 적은 기능에는 region을 만들지 않는다
- region을 중첩하지 않는다
- region이 지나치게 커지면 region을 추가하기보다 클래스 분리를 먼저 검토한다
- `#pragma region` 다음과 `#pragma endregion` 앞에는 빈 줄을 둔다

region은 C++ 스코프를 생성하지 않으므로 이전 코드의 접근 상태에 의존하지 않는다.

- 각 region은 첫 번째 멤버의 접근 지정자를 반드시 명시한다
- 각 region 안에서 함수 그룹을 먼저 작성하고 변수 그룹을 나중에 작성한다
- 함수 그룹과 변수 그룹은 각각 `public`, `protected`, `private` 순서를 따른다
- region이 끝난 뒤 멤버를 작성할 때도 접근 지정자를 다시 명시한다

```cpp
#pragma region Input

public:
	/** 플레이어 입력 컴포넌트에 입력 액션을 바인딩합니다 */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_AbilityTagPressed(FGameplayTag InputTag);
	void Input_AbilityTagReleased(FGameplayTag InputTag);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<URSInputConfig> InputConfig;

#pragma endregion
```
