#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSTeleporter.generated.h"

class ARSPlayerCharacter;
class UArrowComponent;
class UBoxComponent;

/**
 * 영역에 진입한 플레이어 캐릭터를 자신이 가진 도착 지점으로 옮깁니다
 * 도착한 뒤의 게임플레이 판정은 도착 지점에 배치된 다른 Actor가 담당합니다
 */
UCLASS()
class RS_API ARSTeleporter : public AActor
{
	GENERATED_BODY()

public:
	/** 플레이어 캐릭터만 감지하는 Trigger 영역과 도착 지점을 구성합니다 */
	ARSTeleporter();

protected:
	/** 서버에서만 진입 감지를 시작합니다 */
	virtual void BeginPlay() override;

protected:
	/** 플레이어 캐릭터의 진입을 감지할 영역입니다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Teleport")
	TObjectPtr<UBoxComponent> TriggerArea;

	/**
	 * 플레이어 캐릭터를 옮길 도착 지점이며 이 컴포넌트의 위치와 Yaw를 사용합니다
	 * 도착 지점이 고정된 지형을 가리켜 Teleporter를 옮겨도 따라오지 않아야 한다면 Transform의 Absolute Location을 사용합니다
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Teleport")
	TObjectPtr<UArrowComponent> DestinationPoint;

private:
	/** 영역에 진입한 플레이어 캐릭터를 도착 지점으로 옮깁니다 */
	UFUNCTION()
	void HandleTriggerAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
