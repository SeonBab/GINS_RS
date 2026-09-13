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

	bool ContainsTargetInSectorSlice(const AActor& Attacker, AActor& Target, const FTransform& LockedAttackTransform, const FRSArmSwingSectorBounds& SectorBounds)
	{
		FRSCombatShape CandidateShape;
		CandidateShape.Type = ERSCombatShapeType::Sphere;
		CandidateShape.Radius = SectorBounds.OuterRadius + FRSArmSwingMath::CandidateQueryRadiusMargin;
		CandidateShape.InnerRadius = 0.0f;

		TArray<AActor*> CandidateTargets;
		URSCombatFunctionLibrary::FindTargetsInShapeWithoutDebugDraw(&Attacker, TargetChannel, CandidateShape, LockedAttackTransform, CandidateTargets);

		return CandidateTargets.Contains(&Target) && FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, SectorBounds, Target.GetActorLocation());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSArmSwingMathTest, "RS.Combat.ArmSwing.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSArmSwingMathTest::RunTest(const FString& Parameters)
{
	FRSArmSwingBoxDefinition BoxDefinition;
	BoxDefinition.InnerOffset = 100.0f;
	BoxDefinition.BoxLength = 400.0f;
	BoxDefinition.BoxHalfWidth = 50.0f;

	FRSArmSwingPathDefinition LeftPath;
	LeftPath.StartYawOffset = -90.0f;
	LeftPath.SweepAngleDegrees = 180.0f;

	TestTrue(TEXT("Valid box definition"), BoxDefinition.IsDataValid());
	TestTrue(TEXT("Valid path definition"), LeftPath.IsDataValid());

	FRSArmSwingBoxDefinition InvalidBoxDefinition = BoxDefinition;
	InvalidBoxDefinition.BoxLength = 0.0f;
	TestFalse(TEXT("Zero box length is invalid"), InvalidBoxDefinition.IsDataValid());

	FRSArmSwingPathDefinition InvalidPath = LeftPath;
	InvalidPath.SweepAngleDegrees = 0.0f;
	TestFalse(TEXT("Zero sweep angle is invalid"), InvalidPath.IsDataValid());

	FTransform LockedAttackTransformFromCapsule;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(10.0f, 20.0f, 130.0f));
	TestTrue(TEXT("Locked attack transform is calculated from capsule bottom"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector(0.0f, 1.0f, 0.0f), LockedAttackTransformFromCapsule));
	TestTrue(TEXT("Locked attack origin uses capsule bottom"), LockedAttackTransformFromCapsule.GetLocation().Equals(FVector(10.0f, 20.0f, 30.0f), 0.01f));
	TestTrue(TEXT("Locked attack yaw uses horizontal actor forward"), LockedAttackTransformFromCapsule.GetRotation().GetForwardVector().Equals(FVector(0.0f, 1.0f, 0.0f), 0.01f));
	TestFalse(TEXT("Vertical actor forward cannot lock attack yaw"), FRSArmSwingMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector::UpVector, LockedAttackTransformFromCapsule));

	const FTransform LockedAttackTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 20.0f, 30.0f));
	FRSArmSwingPathDefinition RightPath;
	RightPath.StartYawOffset = -LeftPath.StartYawOffset;
	RightPath.SweepAngleDegrees = -LeftPath.SweepAngleDegrees;

	FRSArmSwingSectorBounds LeftBounds;
	FRSArmSwingSectorBounds RightBounds;
	TestTrue(TEXT("Left telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(BoxDefinition, LeftPath, LeftBounds));
	TestTrue(TEXT("Right telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(BoxDefinition, RightPath, RightBounds));
	TestTrue(TEXT("Telegraph inner radius preserves the empty center"), FMath::IsNearlyEqual(LeftBounds.InnerRadius, 100.0f));
	TestTrue(TEXT("Telegraph outer radius includes the outer corner"), FMath::IsNearlyEqual(LeftBounds.OuterRadius, FMath::Sqrt(FMath::Square(500.0f) + FMath::Square(50.0f))));
	const float ExpectedAngularPadding = FMath::RadiansToDegrees(FMath::Atan2(50.0f, 300.0f));
	TestTrue(TEXT("Telegraph padding uses the rotating box center radius"), FMath::IsNearlyEqual(LeftBounds.StartYawOffset, LeftPath.StartYawOffset - ExpectedAngularPadding));
	TestTrue(TEXT("Telegraph sweep includes the box width at both ends"), FMath::IsNearlyEqual(LeftBounds.SweepAngleDegrees, LeftPath.SweepAngleDegrees + ExpectedAngularPadding * 2.0f));
	TestTrue(TEXT("Mirrored telegraph start is symmetric"), FMath::IsNearlyEqual(RightBounds.StartYawOffset, -LeftBounds.StartYawOffset));
	TestTrue(TEXT("Mirrored telegraph sweep is symmetric"), FMath::IsNearlyEqual(RightBounds.SweepAngleDegrees, -LeftBounds.SweepAngleDegrees));

	FRSArmSwingBoxDefinition PivotTouchingBox = BoxDefinition;
	PivotTouchingBox.InnerOffset = 0.0f;
	PivotTouchingBox.BoxLength = 500.0f;
	PivotTouchingBox.BoxHalfWidth = 100.0f;
	FRSArmSwingPathDefinition PivotTouchingPath;
	PivotTouchingPath.StartYawOffset = 30.0f;
	PivotTouchingPath.SweepAngleDegrees = 160.0f;
	FRSArmSwingSectorBounds PivotTouchingBounds;
	TestTrue(TEXT("Pivot touching box telegraph bounds are calculated"), FRSArmSwingMath::TryCalculateTelegraphBounds(PivotTouchingBox, PivotTouchingPath, PivotTouchingBounds));
	const float PivotTouchingPadding = FMath::RadiansToDegrees(FMath::Atan2(100.0f, 250.0f));
	TestTrue(TEXT("Pivot touching box avoids near full circle padding"), FMath::IsNearlyEqual(PivotTouchingBounds.SweepAngleDegrees, 160.0f + PivotTouchingPadding * 2.0f));
	TestTrue(TEXT("Pivot touching box keeps one readable sector"), PivotTouchingBounds.SweepAngleDegrees < 210.0f);

	FRSArmSwingSectorBounds PartialSectorBounds;
	TestTrue(TEXT("Partial attack sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(BoxDefinition, LeftPath, 0.25f, 0.5f, PartialSectorBounds));
	TestTrue(TEXT("Partial sector begins with width before its previous angle"), FMath::IsNearlyEqual(PartialSectorBounds.StartYawOffset, -45.0f - ExpectedAngularPadding));
	TestTrue(TEXT("Partial sector covers the elapsed angle and both width margins"), FMath::IsNearlyEqual(PartialSectorBounds.SweepAngleDegrees, 45.0f + ExpectedAngularPadding * 2.0f));
	FRSArmSwingSectorBounds InvalidSectorBounds;
	TestFalse(TEXT("Reversed progress interval is invalid"), FRSArmSwingMath::TryCalculateSectorBounds(BoxDefinition, LeftPath, 0.5f, 0.25f, InvalidSectorBounds));

	FRSArmSwingSectorBounds StartSectorBounds;
	TestTrue(TEXT("Stationary start sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(BoxDefinition, LeftPath, 0.0f, 0.0f, StartSectorBounds));
	TestTrue(TEXT("Stationary start sector keeps the attack width"), FMath::IsNearlyEqual(StartSectorBounds.SweepAngleDegrees, ExpectedAngularPadding * 2.0f));

	const FVector PartialMiddleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * 300.0f);
	const FVector PartialOutsideAngleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, 20.0f, 0.0f).Vector() * 300.0f);
	const FVector PartialInsideRadiusLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * 99.0f);
	const FVector PartialOuterBoundaryLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * PartialSectorBounds.OuterRadius);
	const FVector PartialOutsideRadiusLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -20.0f, 0.0f).Vector() * (PartialSectorBounds.OuterRadius + 1.0f));
	const FVector PartialDifferentHeightLocation = PartialMiddleLocation + FVector(0.0f, 0.0f, 1000.0f);
	TestTrue(TEXT("Target center inside partial sector is included"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialMiddleLocation));
	TestFalse(TEXT("Target center outside partial angle is excluded"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialOutsideAngleLocation));
	TestFalse(TEXT("Target center inside inner radius is excluded"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialInsideRadiusLocation));
	TestTrue(TEXT("Target center on outer boundary is included"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialOuterBoundaryLocation));
	TestFalse(TEXT("Target center outside outer radius is excluded"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialOutsideRadiusLocation));
	TestTrue(TEXT("Flat combat sector ignores target height"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, PartialSectorBounds, PartialDifferentHeightLocation));

	FRSArmSwingSectorBounds MirroredPartialSectorBounds;
	TestTrue(TEXT("Mirrored partial attack sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(BoxDefinition, RightPath, 0.25f, 0.5f, MirroredPartialSectorBounds));
	const FVector MirroredMiddleLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, 20.0f, 0.0f).Vector() * 300.0f);
	TestTrue(TEXT("Negative sweep includes its directed partial sector"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, MirroredPartialSectorBounds, MirroredMiddleLocation));
	TestFalse(TEXT("Negative sweep excludes the opposite partial sector"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, MirroredPartialSectorBounds, PartialMiddleLocation));

	FRSArmSwingPathDefinition WrappedPath;
	WrappedPath.StartYawOffset = 170.0f;
	WrappedPath.SweepAngleDegrees = 30.0f;
	FRSArmSwingSectorBounds WrappedBounds;
	TestTrue(TEXT("Wrapped sector is calculated"), FRSArmSwingMath::TryCalculateSectorBounds(BoxDefinition, WrappedPath, 0.0f, 1.0f, WrappedBounds));
	const FVector WrappedInsideLocation = LockedAttackTransform.TransformPosition(FRotator(0.0f, -175.0f, 0.0f).Vector() * 300.0f);
	TestTrue(TEXT("Sector containment supports the signed angle wrap"), FRSArmSwingMath::IsLocationInsideSector(LockedAttackTransform, WrappedBounds, WrappedInsideLocation));

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

	Target->SetActorLocation(PartialOuterBoundaryLocation);
	TestTrue(TEXT("Candidate query margin preserves the final outer boundary"), RSArmSwingMathTest::ContainsTargetInSectorSlice(*Attacker, *Target, LockedAttackTransform, PartialSectorBounds));
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
