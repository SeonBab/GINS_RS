#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSTeleporter.generated.h"

class ARSPlayerCharacter;
class UBoxComponent;

/**
 * 영역에 진입한 플레이어 캐릭터를 지정한 목적지 Actor의 위치로 옮깁니다
 * 도착한 뒤의 게임플레이 판정은 목적지에 배치된 다른 Actor가 담당합니다
 */
UCLASS()
class RS_API ARSTeleporter : public AActor
{
	GENERATED_BODY()

public:
	/** 플레이어 캐릭터만 감지하는 Trigger 영역을 구성합니다 */
	ARSTeleporter();

protected:
	/** 목적지 설정을 확인하고 서버에서만 진입 감지를 시작합니다 */
	virtual void BeginPlay() override;

protected:
	/** 플레이어 캐릭터의 진입을 감지할 영역입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Teleport")
	TObjectPtr<UBoxComponent> TriggerArea;

	/** 플레이어 캐릭터를 옮길 목적지이며 이 Actor의 위치와 Yaw를 사용합니다 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RS|Teleport")
	TObjectPtr<AActor> DestinationActor;

private:
	/** 영역에 진입한 플레이어 캐릭터를 목적지로 옮깁니다 */
	UFUNCTION()
	void HandleTriggerAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
