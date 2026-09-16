#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include <limits>

#include "Combat/RSCombatFunctionLibrary.h"
#include "Combat/RSArmSwingMath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RSCombatShapeTestTypes.h"

namespace RSArmSwingMathTest
{
	constexpr ECollisionChannel TargetChannel = ECC_GameTraceChannel2;

	bool ContainsTargetInSectorSlice(const AActor& Attacker, AActor& Target, const FTransform& LockedAttackTransform, const FRSAnnularSectorBounds& SectorBounds)
	{
		FRSCombatShape CandidateShape;
		CandidateShape.Type = ERSCombatShapeType::AnnularSector;
		CandidateShape.OuterRadius = SectorBounds.OuterRadius + FRSArmSwingMath::CandidateQueryRadiusMargin;
		CandidateShape.InnerRadius = 0.0f;

		TArray<AActor*> CandidateTargets;
		URSCombatFunctionLibrary::FindTargetsInShapeWithoutDebugDraw(&Attacker, TargetChannel, CandidateShape, LockedAttackTransform, CandidateTargets);

		return CandidateTargets.Contains(&Target) && URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, SectorBounds, Target.GetActorLocation());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSArmSwingMathTest, "RS.Combat.ArmSwing.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSArmSwingMathTest::RunTest(const FString& Parameters)
{
	FRSArmSwingSectorDefinition SectorDefinition;
	SectorDefinition.InnerRadius = 100.0f;
	SectorDefinition.OuterRadius = 500.0f;
	SectorDefinition.AngularWidthDegrees = 20.0f;

	FRSArmSwingPathDefinition LeftPath;
	LeftPath.StartYawOffset = -90.0f;
	LeftPath.TravelAngleDegrees = 180.0f;

	TestTrue(TEXT("Valid sector definition"), SectorDefinition.IsDataValid());
	TestTrue(TEXT("Valid path definition"), LeftPath.IsDataValid());

	FRSArmSwingSectorDefinition InvalidSectorDefinition = SectorDefinition;
	InvalidSectorDefinition.OuterRadius = InvalidSectorDefinition.InnerRadius;
	TestFalse(TEXT("Outer radius equal to inner radius is invalid"), InvalidSectorDefinition.IsDataValid());
	InvalidSectorDefinition = SectorDefinition;
	InvalidSectorDefinition.AngularWidthDegrees = 0.0f;
	TestFalse(TEXT("Zero angular width is invalid"), InvalidSectorDefinition.IsDataValid());

	FRSArmSwingPathDefinition InvalidPath = LeftPath;
	InvalidPath.TravelAngleDegrees = 0.0f;
	TestFalse(TEXT("Zero travel angle is invalid"), InvalidPath.IsDataValid());

	FTransform LockedAttackTransformFromCapsule;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(10.0f, 20.0f, 130.0f));
	TestTrue(TEXT("Locked attack transform is calculated from capsule bottom"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector(0.0f, 1.0f, 0.0f), LockedAttackTransformFromCapsule));
	TestTrue(TEXT("Locked attack origin uses capsule bottom"), LockedAttackTransformFromCapsule.GetLocation().Equals(FVector(10.0f, 20.0f, 30.0f), 0.01f));
	TestTrue(TEXT("Locked attack yaw uses horizontal actor forward"), LockedAttackTransformFromCapsule.GetRotation().GetForwardVector().Equals(FVector(0.0f, 1.0f, 0.0f), 0.01f));
	TestFalse(TEXT("Vertical actor forward cannot lock attack yaw"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector::UpVector, LockedAttackTransformFromCapsule));

	const FTransform LockedAttackTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 20.0f, 30.0f));
	FRSArmSwingPathDefinition RightPath;
	RightPath.StartYawOffset = -LeftPath.StartYawOffset;
	RightPath.TravelAngleDegrees = -LeftPath.TravelAngleDegrees;

	FRSAnnularSectorBounds LeftBounds;
	FRSAnnularSectorBounds RightBounds;
	TestTrue(TEXT("Left telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(SectorDefinition, LeftPath, 0.0f, LeftBounds));
	TestTrue(TEXT("Right telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(SectorDefinition, RightPath, 0.0f, RightBounds));
	TestTrue(TEXT("Telegraph inner radius preserves the empty center"), FMath::IsNearlyEqual(LeftBounds.InnerRadius, 100.0f));
	TestTrue(TEXT("Telegraph outer radius matches the configured boundary"), FMath::IsNearlyEqual(LeftBounds.OuterRadius, SectorDefinition.OuterRadius));
	TestTrue(TEXT("Telegraph starts exactly at the configured boundary"), FMath::IsNearlyEqual(LeftBounds.StartYawOffset, LeftPath.StartYawOffset));
	TestTrue(TEXT("Telegraph includes travel and instantaneous angular width"), FMath::IsNearlyEqual(LeftBounds.SweepAngleDegrees, LeftPath.TravelAngleDegrees + SectorDefinition.AngularWidthDegrees));
	TestTrue(TEXT("Mirrored telegraph start is symmetric"), FMath::IsNearlyEqual(RightBounds.StartYawOffset, -LeftBounds.StartYawOffset));
	TestTrue(TEXT("Mirrored telegraph sweep is symmetric"), FMath::IsNearlyEqual(RightBounds.SweepAngleDegrees, -LeftBounds.SweepAngleDegrees));

	FRSArmSwingSectorDefinition PivotTouchingSector = SectorDefinition;
	PivotTouchingSector.InnerRadius = 0.0f;
	PivotTouchingSector.OuterRadius = 500.0f;
	PivotTouchingSector.AngularWidthDegrees = 40.0f;
	FRSArmSwingPathDefinition PivotTouchingPath;
	PivotTouchingPath.StartYawOffset = 30.0f;
	PivotTouchingPath.TravelAngleDegrees = 160.0f;
	FRSAnnularSectorBounds PivotTouchingBounds;
	TestTrue(TEXT("Pivot touching sector telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(PivotTouchingSector, PivotTouchingPath, 0.0f, PivotTouchingBounds));
	TestTrue(TEXT("Pivot touching sector uses its configured angular width"), FMath::IsNearlyEqual(PivotTouchingBounds.SweepAngleDegrees, 200.0f));
	FRSArmSwingPathDefinition ZeroStartPath;
	ZeroStartPath.StartYawOffset = 0.0f;
	ZeroStartPath.TravelAngleDegrees = 160.0f;
	FRSAnnularSectorBounds ZeroStartBounds;
	TestTrue(TEXT("Zero start path bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(PivotTouchingSector, ZeroStartPath, 0.0f, ZeroStartBounds));
	TestTrue(TEXT("Zero start path does not extend before zero degrees"), FMath::IsNearlyZero(ZeroStartBounds.StartYawOffset));
	const FVector ZeroStartInsideLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, 1.0f, 0.0f).Vector() * 250.0f);
	const FVector ZeroStartBeforeBoundaryLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -1.0f, 0.0f).Vector() * 250.0f);
	TestTrue(TEXT("Zero start path includes the travel direction side"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, ZeroStartBounds, ZeroStartInsideLocation));
	TestFalse(TEXT("Zero start path excludes the side before zero degrees"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, ZeroStartBounds, ZeroStartBeforeBoundaryLocation));

	// 예고와 판정이 같은 계산을 지나므로 하한도 여기서 한 번만 걸립니다
	FRSAnnularSectorBounds FlooredTelegraphBounds;
	TestTrue(TEXT("Floored telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(PivotTouchingSector, PivotTouchingPath, 120.0f, FlooredTelegraphBounds));
	TestTrue(TEXT("Floored telegraph bounds start at the floor"), FMath::IsNearlyEqual(FlooredTelegraphBounds.InnerRadius, 120.0f));
	TestTrue(TEXT("Floored telegraph bounds keep their angle"), FMath::IsNearlyEqual(FlooredTelegraphBounds.SweepAngleDegrees, 200.0f));

	FRSAnnularSectorBounds FlooredSectorBounds;
	TestTrue(TEXT("Floored attack sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(PivotTouchingSector, PivotTouchingPath, 0.25f, 0.5f, 120.0f, FlooredSectorBounds));
	TestTrue(TEXT("Floored attack sector starts at the floor"), FMath::IsNearlyEqual(FlooredSectorBounds.InnerRadius, 120.0f));

	// 기획이 적은 안쪽 반지름이 하한보다 크면 그 값이 이깁니다. 하한은 바닥이지 덮어쓰기가 아닙니다
	FRSAnnularSectorBounds AuthoredInnerBounds;
	TestTrue(TEXT("A floor below the authored inner radius is calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(SectorDefinition, LeftPath, 50.0f, AuthoredInnerBounds));
	TestTrue(TEXT("A floor below the authored inner radius changes nothing"), FMath::IsNearlyEqual(AuthoredInnerBounds.InnerRadius, 100.0f));

	// 부채꼴 전체가 보스 안에 들어가면 판정할 면적이 없으므로 그 구간은 아무도 맞히지 않아야 합니다
	FRSAnnularSectorBounds SwallowedSectorBounds;
	TestFalse(TEXT("A floor beyond the outer radius leaves no sector"), FRSArmSwingMath::TryCalculateSectorBounds(PivotTouchingSector, PivotTouchingPath, 0.25f, 0.5f, PivotTouchingSector.OuterRadius, SwallowedSectorBounds));

	FRSAnnularSectorBounds PartialSectorBounds;
	TestTrue(TEXT("Partial attack sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, LeftPath, 0.25f, 0.5f, 0.0f, PartialSectorBounds));
	TestTrue(TEXT("Partial sector begins exactly at its previous path boundary"), FMath::IsNearlyEqual(PartialSectorBounds.StartYawOffset, -45.0f));
	TestTrue(TEXT("Partial sector covers the elapsed travel and instantaneous angular width"), FMath::IsNearlyEqual(PartialSectorBounds.SweepAngleDegrees, 45.0f + SectorDefinition.AngularWidthDegrees));
	FRSAnnularSectorBounds InvalidSectorBounds;
	TestFalse(TEXT("Reversed progress interval is invalid"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, LeftPath, 0.5f, 0.25f, 0.0f, InvalidSectorBounds));

	FRSAnnularSectorBounds StartSectorBounds;
	TestTrue(TEXT("Stationary start sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, LeftPath, 0.0f, 0.0f, 0.0f, StartSectorBounds));
	TestTrue(TEXT("Stationary start sector begins at the configured boundary"), FMath::IsNearlyEqual(StartSectorBounds.StartYawOffset, LeftPath.StartYawOffset));
	TestTrue(TEXT("Stationary start sector keeps its configured angular width"), FMath::IsNearlyEqual(StartSectorBounds.SweepAngleDegrees, SectorDefinition.AngularWidthDegrees));
	FRSAnnularSectorBounds MirroredStartSectorBounds;
	TestTrue(TEXT("Mirrored stationary start sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, RightPath, 0.0f, 0.0f, 0.0f, MirroredStartSectorBounds));
	TestTrue(TEXT("Negative travel starts exactly at the configured boundary"), FMath::IsNearlyEqual(MirroredStartSectorBounds.StartYawOffset, RightPath.StartYawOffset));
	TestTrue(TEXT("Negative travel keeps its width in the travel direction"), FMath::IsNearlyEqual(MirroredStartSectorBounds.SweepAngleDegrees, -SectorDefinition.AngularWidthDegrees));

	const FVector PartialMiddleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * 300.0f);
	const FVector PartialOutsideAngleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, 21.0f, 0.0f).Vector() * 300.0f);
	const FVector PartialInsideRadiusLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * 99.0f);
	const FVector PartialJustInsideOuterLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * (PartialSectorBounds.OuterRadius - 1.0f));
	const FVector PartialOutsideRadiusLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * (PartialSectorBounds.OuterRadius + 1.0f));
	const FVector PartialDifferentHeightLocation = PartialMiddleLocation + FVector(0.0f, 0.0f, 1000.0f);
	TestTrue(TEXT("Target center inside partial sector is included"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialMiddleLocation));
	TestFalse(TEXT("Target center outside partial angle is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialOutsideAngleLocation));
	TestFalse(TEXT("Target center inside inner radius is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialInsideRadiusLocation));
	// 반지름이 반개구간이라 바깥 경계는 바깥쪽 형상의 것입니다. 경계에 정확히 선 대상의 소유는 회전 왕복 오차를 타지 않는 RS.Combat.AnnularSector가 검사합니다
	TestTrue(TEXT("Target center just inside the outer boundary is included"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialJustInsideOuterLocation));
	TestFalse(TEXT("Target center outside outer radius is excluded"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialOutsideRadiusLocation));
	TestTrue(TEXT("Flat combat sector ignores target height"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, PartialSectorBounds, PartialDifferentHeightLocation));

	FRSAnnularSectorBounds MirroredPartialSectorBounds;
	TestTrue(TEXT("Mirrored partial attack sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, RightPath, 0.25f, 0.5f, 0.0f, MirroredPartialSectorBounds));
	const FVector MirroredMiddleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, 20.0f, 0.0f).Vector() * 300.0f);
	const FVector MirroredOutsideLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -21.0f, 0.0f).Vector() * 300.0f);
	TestTrue(TEXT("Negative travel includes its directed partial sector"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, MirroredPartialSectorBounds, MirroredMiddleLocation));
	TestFalse(TEXT("Negative travel excludes the opposite partial sector"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, MirroredPartialSectorBounds, MirroredOutsideLocation));

	FRSArmSwingPathDefinition WrappedPath;
	WrappedPath.StartYawOffset = 170.0f;
	WrappedPath.TravelAngleDegrees = 30.0f;
	FRSAnnularSectorBounds WrappedBounds;
	TestTrue(TEXT("Wrapped sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(SectorDefinition, WrappedPath, 0.0f, 1.0f, 0.0f, WrappedBounds));
	const FVector WrappedInsideLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -175.0f, 0.0f).Vector() * 300.0f);
	TestTrue(TEXT("Sector containment supports the signed angle wrap"), URSCombatFunctionLibrary::IsLocationInsideAnnularSector(LockedAttackTransform, WrappedBounds, WrappedInsideLocation));

	FVector TargetTangentDirection;
	TestTrue(TEXT("Target tangent is calculated from its radial direction"), FRSArmSwingMath::TryCalculateTargetTangentDirection(LockedAttackTransform, LeftPath, 0.5f, PartialMiddleLocation, TargetTangentDirection));
	const FVector ExpectedTargetTangent = (PartialMiddleLocation - LockedAttackTransform.GetLocation()).GetSafeNormal2D().RotateAngleAxis(90.0f, FVector::UpVector);
	TestTrue(TEXT("Positive sweep target tangent follows counterclockwise movement"), TargetTangentDirection.Equals(ExpectedTargetTangent, 0.01f));

	const TArray<float> LinearProgress = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	TestTrue(TEXT("Linear progress samples are valid"), FRSArmSwingMath::IsProgressSampleSequenceValid(LinearProgress));

	const TArray<float> EasedProgress = { 0.0f, 0.05f, 0.1f, 0.6f, 0.95f, 1.0f };
	TestTrue(TEXT("Accelerating progress samples are valid"), FRSArmSwingMath::IsProgressSampleSequenceValid(EasedProgress));

	const TArray<float> HeldProgress = { 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f };
	TestTrue(TEXT("Progress samples may hold still"), FRSArmSwingMath::IsProgressSampleSequenceValid(HeldProgress));

	FString ProgressValidationError;
	const TArray<float> OffsetStartProgress = { 0.2f, 0.5f, 1.0f };
	TestFalse(TEXT("Progress must start at zero"), FRSArmSwingMath::IsProgressSampleSequenceValid(OffsetStartProgress, &ProgressValidationError));
	TestFalse(TEXT("Offset start reports an error"), ProgressValidationError.IsEmpty());

	const TArray<float> ShortEndProgress = { 0.0f, 0.5f, 0.8f };
	TestFalse(TEXT("Progress must end at one"), FRSArmSwingMath::IsProgressSampleSequenceValid(ShortEndProgress));

	const TArray<float> RewindingProgress = { 0.0f, 0.6f, 0.3f, 1.0f };
	TestFalse(TEXT("Progress must not rewind"), FRSArmSwingMath::IsProgressSampleSequenceValid(RewindingProgress));

	const TArray<float> MissingCurveProgress = { 0.0f, 0.0f, 0.0f };
	TestFalse(TEXT("A curve that never advances is invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(MissingCurveProgress));

	const TArray<float> SingleProgress = { 0.0f };
	TestFalse(TEXT("A single progress sample is invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(SingleProgress));

	const TArray<float> NonFiniteProgress = { 0.0f, std::numeric_limits<float>::infinity(), 1.0f };
	TestFalse(TEXT("Non-finite progress samples are invalid"), FRSArmSwingMath::IsProgressSampleSequenceValid(NonFiniteProgress));

	const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("RSArmSwingMathTestWorld"), EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
	TestWorld->AddToRoot();
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	AActor* Attacker = TestWorld->SpawnActor<AActor>();
	ARSCombatShapeTestActor* Target = TestWorld->SpawnActor<ARSCombatShapeTestActor>();

	// 후보 수집과 최종 판정이 바깥 경계를 각자 계산하므로, 경계 바로 안쪽의 대상이 후보 단계에서 미리 탈락하지 않아야 합니다
	Target->SetActorLocation(PartialJustInsideOuterLocation);
	TestTrue(TEXT("Candidate query margin preserves a target just inside the outer boundary"), RSArmSwingMathTest::ContainsTargetInSectorSlice(*Attacker, *Target, LockedAttackTransform, PartialSectorBounds));
	Target->SetActorLocation(LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * (PartialSectorBounds.OuterRadius + 0.5f)));
	TestFalse(TEXT("Final sector rejects a candidate inside only the query margin"), RSArmSwingMathTest::ContainsTargetInSectorSlice(*Attacker, *Target, LockedAttackTransform, PartialSectorBounds));
	Target->SetActorLocation(PartialOutsideAngleLocation);
	TestFalse(TEXT("Final sector rejects a broad-phase candidate outside the slice angle"), RSArmSwingMathTest::ContainsTargetInSectorSlice(*Attacker, *Target, LockedAttackTransform, PartialSectorBounds));
	Target->SetActorLocation(MirroredMiddleLocation);
	TestTrue(TEXT("Candidate query and final filter preserve a negative sweep slice"), RSArmSwingMathTest::ContainsTargetInSectorSlice(*Attacker, *Target, LockedAttackTransform, MirroredPartialSectorBounds));

	TestWorld->RemoveFromRoot();
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->DestroyWorld(false);

	return true;
}

#endif
