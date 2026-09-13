#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSHitFlashComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class URSHealthComponent;

/**
 * 실제 체력 감소에 반응해 대상 Mesh에 짧은 Overlay 피격 플래시를 재생합니다
 * 공격, 보스 상태와 원본 Material을 알지 않고 Health 통지와 시각 효과 수명만 소유합니다
 */
UCLASS(ClassGroup = (RS), meta = (BlueprintSpawnableComponent))
class RS_API URSHitFlashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URSHitFlashComponent();

	/** 활성화된 플래시가 있을 때만 밝기를 감쇠합니다 */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Health 구독과 Overlay를 제거하고 피격 전 상태를 복원합니다 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 런타임 중 컴포넌트가 직접 제거되는 경로에서도 Overlay를 복원합니다 */
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

public:
	/** 실제 체력 감소를 감시할 HealthComponent와 Overlay를 적용할 Mesh를 연결합니다 */
	void Initialize(URSHealthComponent* InHealthComponent, UMeshComponent* InTargetMeshComponent);

	/** 기존 Health 구독과 진행 중인 플래시를 정리합니다 */
	void Uninitialize();

private:
	/** Health가 실제로 감소했을 때 플래시를 최대 밝기부터 다시 시작합니다 */
	UFUNCTION()
	void HandleHealthChanged(URSHealthComponent* HealthComponent, float OldValue, float NewValue);

	/** 설정된 원본 Material에서 재사용할 Dynamic Material Instance를 준비합니다 */
	bool EnsureMaterialInstance();

	/** 현재 Overlay와 Tick을 정리하고 피격 전에 사용하던 Overlay를 복원합니다 */
	void ResetHitFlash();

private:
	/** Fresnel 계산과 밝기 파라미터를 제공하는 피격 플래시 Material입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Hit Flash", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> HitFlashMaterial;

	/** 최대 밝기부터 0까지 감쇠하는 시간입니다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RS|Hit Flash", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", ForceUnits = "s"))
	float HitFlashDuration = 0.12f;

	/** 실제 체력 감소를 전달하는 컴포넌트입니다 */
	UPROPERTY(Transient)
	TObjectPtr<URSHealthComponent> ObservedHealthComponent;

	/** 플래시 Overlay를 적용할 Mesh입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> TargetMeshComponent;

	/** HitFlashAmount를 매 재생 갱신하기 위해 재사용하는 Material Instance입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitFlashMaterialInstance;

	/** 플래시가 덮기 전에 Mesh가 사용하던 Overlay Material입니다 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousOverlayMaterial;

	float HitFlashElapsed = 0.0f;
	bool bIsHitFlashActive = false;
};
