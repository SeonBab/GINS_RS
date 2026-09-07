// Fill out your copyright notice in the Description page of Project Settings.


#include "RSDamageNumberViewModel.h"

#include "RSAbilitySystemComponent.h"
#include "RSPlayerCharacter.h"

void URSDamageNumberViewModel::BeginDestroy()
{
	// 소멸 중에는 표현을 정리할 구독자도 함께 사라지므로 배선만 해제합니다
	DisconnectDamageSource();

	Super::BeginDestroy();
}

void URSDamageNumberViewModel::HandleSourceRegistered(UObject* Source)
{
	ARSPlayerCharacter* PlayerCharacter = Cast<ARSPlayerCharacter>(Source);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	// ASC가 같아 배선을 유지하는 경우에도 Source는 새 Pawn으로 갱신해야
	// 이전 Pawn의 늦은 해제가 현재 구독을 끊지 않습니다
	CurrentSource = Source;

	InitializeViewModel(Cast<URSAbilitySystemComponent>(PlayerCharacter->GetAbilitySystemComponent()));
}

void URSDamageNumberViewModel::HandleSourceUnregistered(UObject* Source)
{
	if (CurrentSource.Get() != Source)
	{
		return;
	}

	CurrentSource.Reset();

	UninitializeViewModel();
}

void URSDamageNumberViewModel::InitializeViewModel(URSAbilitySystemComponent* InAbilitySystemComponent)
{
	// Pawn 소유 시점에는 PlayerState와 ASC가 아직 없을 수 있습니다
	// 이후 같은 Source가 다시 등록되어 해소되는 정상 경로이므로 경고를 남기지 않습니다
	if (!IsValid(InAbilitySystemComponent))
	{
		return;
	}

	if (ConnectedAbilitySystemComp.Get() == InAbilitySystemComponent)
	{
		return;
	}

	// 이전 소유자의 숫자가 새 원본의 것으로 보이지 않도록 교체 시점에 표현을 폐기합니다
	const bool bHadPreviousSource = !ConnectedAbilitySystemComp.IsExplicitlyNull();

	ConnectDamageSource(InAbilitySystemComponent);

	if (bHadPreviousSource)
	{
		OnDamageNumberResetRequested.Broadcast();
	}
}

void URSDamageNumberViewModel::UninitializeViewModel()
{
	// 약한 참조가 만료된 경우에도 화면의 숫자는 정리해야 하므로 유효성이 아니라 설정 여부로 판정합니다
	if (ConnectedAbilitySystemComp.IsExplicitlyNull())
	{
		return;
	}

	DisconnectDamageSource();

	OnDamageNumberResetRequested.Broadcast();
}

void URSDamageNumberViewModel::ConnectDamageSource(URSAbilitySystemComponent* InAbilitySystemComponent)
{
	// 목표는 중복 방지가 아니라 현재 원본과 정확히 하나의 연결만 남기는 것이므로 이전 연결을 먼저 끊습니다
	DisconnectDamageSource();

	ConnectedAbilitySystemComp = InAbilitySystemComponent;
	InAbilitySystemComponent->OnDamageDealt.AddUniqueDynamic(this, &ThisClass::HandleDamageDealt);
}

void URSDamageNumberViewModel::DisconnectDamageSource()
{
	if (URSAbilitySystemComponent* AbilitySystemComp = ConnectedAbilitySystemComp.Get())
	{
		AbilitySystemComp->OnDamageDealt.RemoveDynamic(this, &ThisClass::HandleDamageDealt);
	}

	ConnectedAbilitySystemComp.Reset();
}

void URSDamageNumberViewModel::HandleDamageDealt(float AppliedDamage, FVector TargetLocation)
{
	// 하한 1이 음수를 1로 만들지 않도록 변환보다 먼저 확인합니다
	if (AppliedDamage <= 0.0f)
	{
		return;
	}

	// RS의 데미지는 논리적으로 정수이며, 하한 1은 양수 피해가 0으로 표시되는 것을 막는 방어적 변환입니다
	const int32 DisplayDamage = FMath::Max(1, FMath::RoundToInt(AppliedDamage));

	// 자릿수 구분 기호는 문화권마다 달라 같은 피해가 다르게 보이므로 전투 숫자에는 사용하지 않습니다
	FNumberFormattingOptions DamageNumberFormat;
	DamageNumberFormat.SetUseGrouping(false);

	const FText DisplayDamageText = FText::AsNumber(DisplayDamage, &DamageNumberFormat);

	// 표시 높이는 표현 결정이므로 게임플레이가 전달한 원좌표에 여기서만 더합니다
	const FVector WorldAnchor = TargetLocation + FVector(0.0f, 0.0f, AnchorHeightOffset);

	OnDamageNumberRequested.Broadcast(DisplayDamageText, WorldAnchor);
}
