#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/Boss/RSGameplayAbility_SequentialSweepExplosion.h"
#include "Combat/RSCombatFunctionLibrary.h"
#include "Combat/RSSequentialSweepExplosionMath.h"
#include "RSGameplayTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSequentialSweepExplosionDefinitionTest, "RS.Combat.SequentialSweepExplosion.Definition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSequentialSweepExplosionDefinitionTest::RunTest(const FString& Parameters)
{
	FRSSequentialSweepExplosionDefinition Definition;
	TestTrue(TEXT("Default definition is valid"), Definition.IsDataValid());

	const URSGameplayAbility_SequentialSweepExplosion* AbilityCDO = GetDefault<URSGameplayAbility_SequentialSweepExplosion>();
	TestTrue(TEXT("Ability has its identifying asset tag"), AbilityCDO && AbilityCDO->GetAssetTags().HasTagExact(RSGameplayTags::Ability_Combat_SequentialSweepExplosion));

	FRSSequentialSweepExplosionDefinition InvalidDefinition = Definition;
	InvalidDefinition.OuterRadius = 0.0f;
	TestFalse(TEXT("Zero outer radius is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.TotalSweepAngleDegrees = 361.0f;
	TestFalse(TEXT("Sweep above full circle is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.SectorCount = 0;
	TestFalse(TEXT("Zero sectors are invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.FirstExplosionDelay = 0.0f;
	TestFalse(TEXT("Zero first explosion delay is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.SectorExplosionInterval = 0.0f;
	TestFalse(TEXT("Zero explosion interval is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.Damage = 10.5f;
	TestFalse(TEXT("Fractional damage is invalid"), InvalidDefinition.IsDataValid());

	InvalidDefinition = Definition;
	InvalidDefinition.TelegraphHideLeadTime = -0.01f;
	TestFalse(TEXT("Negative telegraph hide lead time is invalid"), InvalidDefinition.IsDataValid());

	// 기본값의 경고 간격은 FirstExplosionDelay 1.5초를 SectorCount 6으로 나눈 0.25초입니다
	InvalidDefinition = Definition;
	InvalidDefinition.TelegraphHideLeadTime = 0.25f;
	TestFalse(TEXT("Telegraph hide lead time at the warning interval is invalid"), InvalidDefinition.IsDataValid());

	FRSSequentialSweepExplosionDefinition HideLeadDefinition = Definition;
	HideLeadDefinition.TelegraphHideLeadTime = 0.24f;
	TestTrue(TEXT("Telegraph hide lead time below the warning interval is valid"), HideLeadDefinition.IsDataValid());

	HideLeadDefinition.TelegraphHideLeadTime = 0.0f;
	TestTrue(TEXT("Hiding at the explosion frame stays valid"), HideLeadDefinition.IsDataValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSequentialSweepExplosionMathTest, "RS.Combat.SequentialSweepExplosion.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSequentialSweepExplosionMathTest::RunTest(const FString& Parameters)
{
	FTransform LockedTransform;
	const FTransform CapsuleTransform(FRotator::ZeroRotator, FVector(100.0f, 200.0f, 300.0f));
	TestTrue(TEXT("Locked transform is calculated"), RSSequentialSweepExplosionMath::TryCalculateLockedAttackTransform(CapsuleTransform, 100.0f, FVector::YAxisVector, LockedTransform));
	TestTrue(TEXT("Locked transform uses capsule bottom"), LockedTransform.GetLocation().Equals(FVector(100.0f, 200.0f, 200.0f)));
	TestTrue(TEXT("Locked transform uses horizontal actor forward"), LockedTransform.GetUnitAxis(EAxis::X).Equals(FVector::YAxisVector));

	TestEqual(TEXT("One hundred twenty degrees split into four sectors"), RSSequentialSweepExplosionMath::CalculateSectorAngleDegrees(120.0f, 4), 30.0f);
	TestEqual(TEXT("Invalid sector count returns zero angle"), RSSequentialSweepExplosionMath::CalculateSectorAngleDegrees(120.0f, 0), 0.0f);

	float SectorStartAngleDegrees = 0.0f;
	float SectorSweepAngleDegrees = 0.0f;
	TestTrue(TEXT("Third sector angles are calculated"), RSSequentialSweepExplosionMath::TryCalculateSectorAngles(120.0f, 4, -60.0f, 2, SectorStartAngleDegrees, SectorSweepAngleDegrees));
	TestEqual(TEXT("Third sector starts at zero degrees"), SectorStartAngleDegrees, 0.0f);
	TestEqual(TEXT("Third sector sweeps thirty degrees"), SectorSweepAngleDegrees, 30.0f);

	TestEqual(TEXT("Timeline starts with one warning"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.0f, 1.0f, 4), 1);
	TestEqual(TEXT("Before first warning boundary only one is visible"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.24f, 1.0f, 4), 1);
	TestEqual(TEXT("Second warning appears at first boundary"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(0.25f, 1.0f, 4), 2);
	TestEqual(TEXT("Large step catches up all warnings"), RSSequentialSweepExplosionMath::CalculateRequiredWarningCount(1.5f, 1.0f, 4), 4);

	TestEqual(TEXT("No explosion occurs before first delay"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(0.99f, 1.0f, 0.2f, 4), 0);
	TestEqual(TEXT("First explosion occurs at first delay"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(1.0f, 1.0f, 0.2f, 4), 1);
	TestEqual(TEXT("Second explosion occurs after one interval"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(1.2f, 1.0f, 0.2f, 4), 2);
	TestEqual(TEXT("Large step catches up all explosions"), RSSequentialSweepExplosionMath::CalculateRequiredExplosionCount(2.0f, 1.0f, 0.2f, 4), 4);
	TestTrue(TEXT("Total duration includes final interval"), FMath::IsNearlyEqual(RSSequentialSweepExplosionMath::CalculateTotalDuration(1.0f, 0.2f, 4), 1.6f));

	const FTransform ForwardLockedTransform(FRotator::ZeroRotator, FVector::ZeroVector);
	// 각도 경계의 소유만 보려는 단언이므로 바깥 반경 경계에서 떨어진 지점을 씁니다
	const FVector SharedBoundaryLocation = FRotator(0.0f, -30.0f, 0.0f).Vector() * 250.0f;
	TestFalse(TEXT("Shared boundary does not belong to the first sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, SharedBoundaryLocation));
	TestTrue(TEXT("Shared boundary belongs to the second sector alone"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 1, SharedBoundaryLocation));

	const FVector FirstSectorMiddleLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 250.0f;
	TestTrue(TEXT("First sector contains its middle"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, FirstSectorMiddleLocation));
	TestFalse(TEXT("Second sector excludes first sector middle"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 1, FirstSectorMiddleLocation));

	const FVector OuterBoundaryLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 500.0f;
	const FVector OutsideRadiusLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 500.1f;
	const FVector InsideOuterBoundaryLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 499.9f;
	TestFalse(TEXT("Outer radius boundary is excluded"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, OuterBoundaryLocation));
	TestTrue(TEXT("Just inside the outer radius is included"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, InsideOuterBoundaryLocation));
	TestFalse(TEXT("Location outside outer radius is excluded"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, OutsideRadiusLocation));
	TestTrue(TEXT("Center belongs to first sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 0, FVector::ZeroVector));
	TestTrue(TEXT("Center also belongs to fourth sector"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 0.0f, 500.0f, 120.0f, 4, -60.0f, 3, FVector::ZeroVector));

	// 보스 캡슐 하한 안쪽은 판정에서 빠지고, 하한 경계에 선 대상은 그대로 맞아야 안전 지대가 생기지 않습니다
	const FVector InsideFloorLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 99.0f;
	const FVector FloorBoundaryLocation = FRotator(0.0f, -45.0f, 0.0f).Vector() * 100.0f;
	TestFalse(TEXT("A location inside the floor is excluded"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 100.0f, 500.0f, 120.0f, 4, -60.0f, 0, InsideFloorLocation));
	TestTrue(TEXT("A location at the floor boundary is included"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 100.0f, 500.0f, 120.0f, 4, -60.0f, 0, FloorBoundaryLocation));
	TestFalse(TEXT("The center is excluded once a floor applies"), RSSequentialSweepExplosionMath::IsLocationInSector(ForwardLockedTransform, 100.0f, 500.0f, 120.0f, 4, -60.0f, 0, FVector::ZeroVector));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRSSequentialSweepExplosionSectorFillTest, "RS.Combat.SequentialSweepExplosion.SectorFill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRSSequentialSweepExplosionSectorFillTest::RunTest(const FString& Parameters)
{
	constexpr float OuterRadius = 700.0f;
	constexpr float TotalSweepAngleDegrees = 160.0f;
	constexpr int32 SectorCount = 4;
	constexpr float StartAngleOffsetDegrees = -80.0f;
	constexpr float Spacing = 90.0f;

	const FTransform LockedTransform(FRotator(0.0f, 25.0f, 0.0f), FVector(300.0f, -150.0f, 60.0f));

	bool bAllSectorsBuildFillCells = true;
	bool bAllFillCellsHitTheirSector = true;
	int32 TotalFillCount = 0;
	for (int32 SectorIndex = 0; SectorIndex < SectorCount; ++SectorIndex)
	{
		FRSCombatShape SectorShape;
		FTransform SectorTransform;
		if (!RSSequentialSweepExplosionMath::TryBuildSectorFillShape(LockedTransform, OuterRadius, TotalSweepAngleDegrees, SectorCount, StartAngleOffsetDegrees, SectorIndex, SectorShape, SectorTransform))
		{
			bAllSectorsBuildFillCells = false;

			continue;
		}

		TArray<FTransform> FillTransforms;
		if (!URSCombatFunctionLibrary::BuildShapeFillTransforms(SectorShape, SectorTransform, Spacing, FillTransforms))
		{
			bAllSectorsBuildFillCells = false;

			continue;
		}

		TotalFillCount += FillTransforms.Num();

		// 연출이 예고한 조각을 벗어나면 폭발 위치를 잘못 알려 주므로 모든 칸이 그 조각의 실제 판정을 통과하는지 확인합니다
		for (const FTransform& FillTransform : FillTransforms)
		{
			if (!RSSequentialSweepExplosionMath::IsLocationInSector(LockedTransform, 0.0f, OuterRadius, TotalSweepAngleDegrees, SectorCount, StartAngleOffsetDegrees, SectorIndex, FillTransform.GetLocation()))
			{
				bAllFillCellsHitTheirSector = false;
			}
		}
	}

	TestTrue(TEXT("Every sector builds fill cells"), bAllSectorsBuildFillCells);
	TestTrue(TEXT("Every sector fill cell passes that sector hit check"), bAllFillCellsHitTheirSector);
	TestTrue(TEXT("Sector fill spreads beyond a single cell per sector"), TotalFillCount > SectorCount);

	// 환형 부채꼴은 한 바퀴까지 표현하므로, Cone 상한 180도 때문에 조용히 실패하던 조각 하나짜리 360도 구성이 이제 성립합니다
	FRSCombatShape FullTurnSectorShape;
	FTransform FullTurnSectorTransform;
	TestTrue(TEXT("A single sector covering a full turn builds a shape"), RSSequentialSweepExplosionMath::TryBuildSectorFillShape(LockedTransform, OuterRadius, 360.0f, 1, 0.0f, 0, FullTurnSectorShape, FullTurnSectorTransform));
	TestTrue(TEXT("A full turn sector covers every angle"), FullTurnSectorShape.GetAnnularSectorBounds().CoversEveryAngle());

	// 시작 각도를 형상이 들고 있으므로 Transform은 고정된 공격 기준을 그대로 씁니다
	FRSCombatShape OffsetSectorShape;
	FTransform OffsetSectorTransform;
	TestTrue(TEXT("An offset sector builds a shape"), RSSequentialSweepExplosionMath::TryBuildSectorFillShape(LockedTransform, OuterRadius, 180.0f, 2, -90.0f, 1, OffsetSectorShape, OffsetSectorTransform));
	TestTrue(TEXT("An offset sector keeps the locked attack rotation"), OffsetSectorTransform.GetRotation().Equals(LockedTransform.GetRotation()));
	TestTrue(TEXT("An offset sector carries its own start angle"), FMath::IsNearlyEqual(OffsetSectorShape.StartYawOffset, 0.0f));

	return true;
}

#endif
