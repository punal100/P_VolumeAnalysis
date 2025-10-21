// Copyright (c) 2025
#include "CPP_AT_VolumeAnalysis__Base.h"
#include "CPP_BPL__VolumeAnalysis.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVolActor, Log, All);

ACPP_AT_VolumeAnalysis_Base::ACPP_AT_VolumeAnalysis_Base()
{
    PrimaryActorTick.bCanEverTick = true;
}

/**
 * Called when the actor begins play
 */
void ACPP_AT_VolumeAnalysis_Base::BeginPlay()
{
    Super::BeginPlay();
}

void ACPP_AT_VolumeAnalysis_Base::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bIsRunning)
    {
        ProcessRowsStep(RowsPerTick);
    }
}

void ACPP_AT_VolumeAnalysis_Base::StartAnalysis()
{
    if (!GetWorld())
    {
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: No World"));
        return;
    }

    // Build AABB from provided volume box
    const FBox AABB = UCPP_BPL__VolumeAnalysis::LinkedBox_GetAABB(VolumeBox);
    if (!AABB.IsValid)
    {
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: Invalid AABB from VolumeBox"));
        return;
    }

    // Generate voxel grid
    PendingBoxes.Reset();
    UCPP_BPL__VolumeAnalysis::GenerateVoxelGridBoxes_ByCounts(AABB, SampleCountX, SampleCountY, SampleCountZ, PendingBoxes);

    GridCountX = SampleCountX;
    GridCountY = SampleCountY;
    GridCountZ = SampleCountZ;

    // Compute approx cell sizes (for overlap radius auto)
    const FVector BoxSize = AABB.GetSize();
    CellSizeX = (GridCountX > 0) ? (BoxSize.X / GridCountX) : 0.f;
    CellSizeY = (GridCountY > 0) ? (BoxSize.Y / GridCountY) : 0.f;
    CellSizeZ = (GridCountZ > 0) ? (BoxSize.Z / GridCountZ) : 0.f;

    // Reset visibility
    for (FS_LinkedBox &B : PendingBoxes)
    {
        B.VisibilityMask = 0;
    }

    // Reset state
    AnalysisResults.Reset();
    VisibleCount = 0;
    HiddenCount = 0;
    bIsSubSampling = false;
    HiddenBoxIndices.Reset();
    CurrentHiddenIndex = 0;
    CurrentCellIndex = 0;
    CurrentPhase = 0;
    CurrentPhaseRowIndex = 0;
    bIsRunning = true;

    if (bDrawDebug && bDrawDebugBox)
    {
        DrawAABB(AABB, FColor::Yellow);
    }
}

void ACPP_AT_VolumeAnalysis_Base::StopAnalysis()
{
    bIsRunning = false;
    bIsSubSampling = false;
}

void ACPP_AT_VolumeAnalysis_Base::ClearResults()
{
    StopAnalysis();
    AnalysisResults.Reset();
    PendingBoxes.Reset();
    VisibleCount = 0;
    HiddenCount = 0;
    GridCountX = GridCountY = GridCountZ = 0;
    CellSizeX = CellSizeY = CellSizeZ = 0.f;
    HiddenBoxIndices.Reset();
    CurrentHiddenIndex = 0;
    CurrentCellIndex = 0;
    CurrentPhase = 0;
    CurrentPhaseRowIndex = 0;
}

TArray<FS_LinkedBox> ACPP_AT_VolumeAnalysis_Base::GetAnalysisResults()
{
    return AnalysisResults;
}

/**
 * Get number of visible points in current analysis
 */
int32 ACPP_AT_VolumeAnalysis_Base::GetVisiblePointCount() const
{
    return VisibleCount;
}

/**
 * Get number of hidden points in current analysis
 */
int32 ACPP_AT_VolumeAnalysis_Base::GetHiddenPointCount() const
{
    return HiddenCount;
}
/**
 * Get visibility percentage (0-100)
 */
float ACPP_AT_VolumeAnalysis_Base::GetVisibilityPercentage() const
{
    const int32 Total = VisibleCount + HiddenCount;
    return (Total > 0) ? (static_cast<float>(VisibleCount) * 100.0f / static_cast<float>(Total)) : 0.0f;
}

void ACPP_AT_VolumeAnalysis_Base::ProcessRowsStep(int32 MaxRowsPerTick)
{
    if (!GetWorld())
    {
        UE_LOG(LogPVolActor, Warning, TEXT("ProcessRowsStep: No World"));
        bIsRunning = false;
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VolumeAnalysis), /*bTraceComplex*/ true);
    if (bIgnoreSelf)
    {
        QueryParams.AddIgnoredActor(this);
    }

    // If we're in sub-sampling mode, prioritize refining hidden boxes
    if (bIsSubSampling)
    {
        UE_LOG(LogPVolActor, VeryVerbose, TEXT("ProcessRowsStep: Entering SubSampling phase with %d remaining"), HiddenBoxIndices.Num() - CurrentHiddenIndex);
        ProcessRowsStep_SubSampling(MaxRowsPerTick, QueryParams);
        return;
    }

    auto GetCenter = [&](const FS_LinkedBox &Box) -> FVector
    {
        return UCPP_BPL__VolumeAnalysis::LinkedBox_GetCenter(Box);
    };

    const auto Index = [&](int32 X, int32 Y, int32 Z) -> int32
    {
        return Z * (GridCountY * GridCountX) + Y * GridCountX + X;
    };

    auto IsCenterFree = [&](const FVector &C) -> bool
    {
        if (!bUseCenterOverlapTest)
        {
            return true;
        }
        const float AutoR = 0.25f * FMath::Max(0.001f, FMath::Min3(CellSizeX, CellSizeY, CellSizeZ));
        const float Radius = (CenterOverlapRadius > 0.f) ? CenterOverlapRadius : AutoR;
        FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
        FHitResult OverlapHit;
        const bool bHitOverlap = GetWorld()->SweepSingleByChannel(OverlapHit, C, C, FQuat::Identity, TraceChannel, Shape, QueryParams);
        return !bHitOverlap;
    };

    // Helpers to scan a row along a principal axis using long traces segmented by hits
    auto ScanRowX = [&](int32 YIndex, int32 ZIndex)
    {
        const int32 Count = GridCountX;
        if (Count <= 0)
            return;
        const auto RowCenter = [&](int32 Xi) -> FVector { return GetCenter(PendingBoxes[Index(Xi, YIndex, ZIndex)]); };

        if (Count == 1)
        {
            const FVector C = RowCenter(0);
            if (IsCenterFree(C))
                PendingBoxes[Index(0, YIndex, ZIndex)].VisibilityMask = 1;
            return;
        }

        const FVector FirstC = RowCenter(0);
        const FVector SecondC = RowCenter(1);
        const float StepLen = FVector::Distance(FirstC, SecondC);
        int32 StartI = 0;
        while (StartI < Count)
        {
            int32 TargetI = Count - 1;
            if (MaxTraceDistance > 0.f && StepLen > KINDA_SMALL_NUMBER)
            {
                const int32 MaxSteps = FMath::Clamp(static_cast<int32>(FMath::FloorToInt(MaxTraceDistance / StepLen)), 1, Count - 1);
                TargetI = FMath::Min(StartI + MaxSteps, Count - 1);
            }
            const FVector StartC = RowCenter(StartI);
            const FVector EndC = RowCenter(TargetI);
            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartC, EndC, TraceChannel, QueryParams);
            if (!bHit)
            {
                for (int32 i = StartI; i <= TargetI; ++i)
                {
                    const FVector C = RowCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(i, YIndex, ZIndex)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), StartC, EndC, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = TargetI + 1;
            }
            else
            {
                const float SegmentLen = FVector::Distance(StartC, EndC);
                const float HitDist = FMath::Clamp(Hit.Time * SegmentLen, 0.f, SegmentLen);
                int32 HitIndex = StartI;
                if (StepLen > KINDA_SMALL_NUMBER)
                {
                    HitIndex = FMath::Clamp(StartI + FMath::FloorToInt(HitDist / StepLen + 1e-3f), StartI, TargetI);
                }
                for (int32 i = StartI; i <= HitIndex; ++i)
                {
                    const FVector C = RowCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(i, YIndex, ZIndex)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    const FVector HitPoint = StartC + (EndC - StartC) * Hit.Time;
                    DrawDebugLine(GetWorld(), StartC, HitPoint, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                    DrawDebugLine(GetWorld(), HitPoint, EndC, FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = FMath::Min(HitIndex + 1, Count);
            }
        }
    };

    auto ScanRowY = [&](int32 XIndex, int32 ZIndex)
    {
        const int32 Count = GridCountY;
        if (Count <= 0)
            return;
        const auto RowCenter = [&](int32 Yi) -> FVector { return GetCenter(PendingBoxes[Index(XIndex, Yi, ZIndex)]); };

        if (Count == 1)
        {
            const FVector C = RowCenter(0);
            if (IsCenterFree(C))
                PendingBoxes[Index(XIndex, 0, ZIndex)].VisibilityMask = 1;
            return;
        }

        const FVector FirstC = RowCenter(0);
        const FVector SecondC = RowCenter(1);
        const float StepLen = FVector::Distance(FirstC, SecondC);
        int32 StartI = 0;
        while (StartI < Count)
        {
            int32 TargetI = Count - 1;
            if (MaxTraceDistance > 0.f && StepLen > KINDA_SMALL_NUMBER)
            {
                const int32 MaxSteps = FMath::Clamp(static_cast<int32>(FMath::FloorToInt(MaxTraceDistance / StepLen)), 1, Count - 1);
                TargetI = FMath::Min(StartI + MaxSteps, Count - 1);
            }
            const FVector StartC = RowCenter(StartI);
            const FVector EndC = RowCenter(TargetI);
            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartC, EndC, TraceChannel, QueryParams);
            if (!bHit)
            {
                for (int32 i = StartI; i <= TargetI; ++i)
                {
                    const FVector C = RowCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(XIndex, i, ZIndex)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), StartC, EndC, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = TargetI + 1;
            }
            else
            {
                const float SegmentLen = FVector::Distance(StartC, EndC);
                const float HitDist = FMath::Clamp(Hit.Time * SegmentLen, 0.f, SegmentLen);
                int32 HitIndex = StartI;
                if (StepLen > KINDA_SMALL_NUMBER)
                {
                    HitIndex = FMath::Clamp(StartI + FMath::FloorToInt(HitDist / StepLen + 1e-3f), StartI, TargetI);
                }
                for (int32 i = StartI; i <= HitIndex; ++i)
                {
                    const FVector C = RowCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(XIndex, i, ZIndex)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    const FVector HitPoint = StartC + (EndC - StartC) * Hit.Time;
                    DrawDebugLine(GetWorld(), StartC, HitPoint, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                    DrawDebugLine(GetWorld(), HitPoint, EndC, FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = FMath::Min(HitIndex + 1, Count);
            }
        }
    };

    auto ScanColumnZ = [&](int32 XIndex, int32 YIndex)
    {
        const int32 Count = GridCountZ;
        if (Count <= 0)
            return;
        const auto ColCenter = [&](int32 Zi) -> FVector { return GetCenter(PendingBoxes[Index(XIndex, YIndex, Zi)]); };

        if (Count == 1)
        {
            const FVector C = ColCenter(0);
            if (IsCenterFree(C))
                PendingBoxes[Index(XIndex, YIndex, 0)].VisibilityMask = 1;
            return;
        }

        const FVector FirstC = ColCenter(0);
        const FVector SecondC = ColCenter(1);
        const float StepLen = FVector::Distance(FirstC, SecondC);
        int32 StartI = 0;
        while (StartI < Count)
        {
            int32 TargetI = Count - 1;
            if (MaxTraceDistance > 0.f && StepLen > KINDA_SMALL_NUMBER)
            {
                const int32 MaxSteps = FMath::Clamp(static_cast<int32>(FMath::FloorToInt(MaxTraceDistance / StepLen)), 1, Count - 1);
                TargetI = FMath::Min(StartI + MaxSteps, Count - 1);
            }
            const FVector StartC = ColCenter(StartI);
            const FVector EndC = ColCenter(TargetI);
            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartC, EndC, TraceChannel, QueryParams);
            if (!bHit)
            {
                for (int32 i = StartI; i <= TargetI; ++i)
                {
                    const FVector C = ColCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(XIndex, YIndex, i)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), StartC, EndC, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = TargetI + 1;
            }
            else
            {
                const float SegmentLen = FVector::Distance(StartC, EndC);
                const float HitDist = FMath::Clamp(Hit.Time * SegmentLen, 0.f, SegmentLen);
                int32 HitIndex = StartI;
                if (StepLen > KINDA_SMALL_NUMBER)
                {
                    HitIndex = FMath::Clamp(StartI + FMath::FloorToInt(HitDist / StepLen + 1e-3f), StartI, TargetI);
                }
                for (int32 i = StartI; i <= HitIndex; ++i)
                {
                    const FVector C = ColCenter(i);
                    if (IsCenterFree(C))
                        PendingBoxes[Index(XIndex, YIndex, i)].VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    const FVector HitPoint = StartC + (EndC - StartC) * Hit.Time;
                    DrawDebugLine(GetWorld(), StartC, HitPoint, FColor::Green, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                    DrawDebugLine(GetWorld(), HitPoint, EndC, FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
                StartI = FMath::Min(HitIndex + 1, Count);
            }
        }
    };

    int32 RowsProcessed = 0;
    while (bIsRunning && RowsProcessed < MaxRowsPerTick && CurrentPhase < 3)
    {
        if (CurrentPhase == 0)
        {
            const int32 TotalRows = GridCountY * GridCountZ;
            if (TotalRows <= 0)
            {
                CurrentPhase = 1;
                CurrentPhaseRowIndex = 0;
                continue;
            }

            const int32 RowIdx = CurrentPhaseRowIndex;
            if (RowIdx >= TotalRows)
            {
                CurrentPhase = 1;
                CurrentPhaseRowIndex = 0;
                continue;
            }
            const int32 ZIndex = RowIdx / GridCountY;
            const int32 YIndex = RowIdx % GridCountY;
            ScanRowX(YIndex, ZIndex);
            ++CurrentPhaseRowIndex;
            ++RowsProcessed;
        }
        else if (CurrentPhase == 1)
        {
            const int32 TotalRows = GridCountX * GridCountZ;
            if (TotalRows <= 0)
            {
                CurrentPhase = 2;
                CurrentPhaseRowIndex = 0;
                continue;
            }
            const int32 RowIdx = CurrentPhaseRowIndex;
            if (RowIdx >= TotalRows)
            {
                CurrentPhase = 2;
                CurrentPhaseRowIndex = 0;
                continue;
            }
            const int32 ZIndex = RowIdx / GridCountX;
            const int32 XIndex = RowIdx % GridCountX;
            ScanRowY(XIndex, ZIndex);
            ++CurrentPhaseRowIndex;
            ++RowsProcessed;
        }
        else if (CurrentPhase == 2)
        {
            const int32 TotalCols = GridCountX * GridCountY;
            if (TotalCols <= 0)
            {
                CurrentPhase = 3;
                break;
            }
            const int32 ColIdx = CurrentPhaseRowIndex;
            if (ColIdx >= TotalCols)
            {
                CurrentPhase = 3;
                break;
            }
            const int32 YIndex = ColIdx / GridCountX;
            const int32 XIndex = ColIdx % GridCountX;
            ScanColumnZ(XIndex, YIndex);
            ++CurrentPhaseRowIndex;
            ++RowsProcessed;
        }
    }

    // progress log
    if (bIsRunning && CurrentPhase < 3)
    {
        UE_LOG(LogPVolActor, VeryVerbose, TEXT("ProcessRowsStep: Phase=%d Row=%d"), CurrentPhase, CurrentPhaseRowIndex);
    }

    const bool bMainCompleted = (CurrentPhase >= 3);
    if (bMainCompleted)
    {
        // Signal main pass complete by setting CurrentCellIndex beyond range for compatibility with downstream logic
        CurrentCellIndex = PendingBoxes.Num();

        if (bEnableSubSampling && !bIsSubSampling)
        {
            HiddenBoxIndices.Reset();
            HiddenBoxIndices.Reserve(PendingBoxes.Num());
            int32 TmpVisible = 0;
            for (int32 i = 0; i < PendingBoxes.Num(); ++i)
            {
                if (PendingBoxes[i].VisibilityMask == 0)
                {
                    HiddenBoxIndices.Add(i);
                }
                else
                {
                    ++TmpVisible;
                }
            }
            const int32 TmpHidden = HiddenBoxIndices.Num();
            bIsSubSampling = TmpHidden > 0;
            CurrentHiddenIndex = 0;
            if (bIsSubSampling)
            {
                bIsRunning = true;
                UE_LOG(LogPVolActor, Display, TEXT("SubSampling: %d hidden boxes to refine (Visible after main=%d, Hidden=%d)"), HiddenBoxIndices.Num(), TmpVisible, TmpHidden);
                const int32 Remaining = MaxRowsPerTick - RowsProcessed;
                if (Remaining > 0)
                {
                    ProcessRowsStep_SubSampling(Remaining, QueryParams);
                }
                else
                {
                    UE_LOG(LogPVolActor, VeryVerbose, TEXT("SubSampling: Deferring to next tick (no remaining budget)"));
                }
                return;
            }
            else
            {
                UE_LOG(LogPVolActor, Display, TEXT("SubSampling: Skipped (no hidden boxes). Main pass: Visible=%d Hidden=%d; bEnableSubSampling=%s"), TmpVisible, TmpHidden, bEnableSubSampling ? TEXT("true") : TEXT("false"));
            }
        }

        if (!bIsSubSampling)
        {
            bIsRunning = false;
            VisibleCount = 0;
            HiddenCount = 0;
            for (const FS_LinkedBox &Box : PendingBoxes)
            {
                if (Box.VisibilityMask)
                    ++VisibleCount;
                else
                    ++HiddenCount;
            }
            AnalysisResults = PendingBoxes;
            if (bDrawDebug && bDrawDebugPoints)
            {
                for (const FS_LinkedBox &Box : AnalysisResults)
                {
                    const FVector C = UCPP_BPL__VolumeAnalysis::LinkedBox_GetCenter(Box);
                    DrawDebugPoint(GetWorld(), C, DebugPointSize, Box.VisibilityMask ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration);
                }
            }
            UE_LOG(LogPVolActor, Display, TEXT("ProcessRowsStep: Complete; boxes=%d (Visible=%d Hidden=%d)"), AnalysisResults.Num(), VisibleCount, HiddenCount);
            OnAnalysisComplete.Broadcast(AnalysisResults);
        }
    }
    else if (bIsSubSampling)
    {
        const int32 Remaining = MaxRowsPerTick - RowsProcessed;
        if (Remaining > 0)
        {
            ProcessRowsStep_SubSampling(Remaining, QueryParams);
        }
        else
        {
            UE_LOG(LogPVolActor, VeryVerbose, TEXT("SubSampling: Skipped this tick (no remaining budget)"));
        }
    }
}

// Sub-sampling tick extension
void ACPP_AT_VolumeAnalysis_Base::ProcessRowsStep_SubSampling(int32 MaxCellsPerTick, const FCollisionQueryParams &QueryParams)
{
    if (!bIsSubSampling || HiddenBoxIndices.Num() == 0)
    {
        return;
    }

    auto GetCenter = [&](const FS_LinkedBox &Box) -> FVector
    {
        return UCPP_BPL__VolumeAnalysis::LinkedBox_GetCenter(Box);
    };

    const auto Index = [&](int32 X, int32 Y, int32 Z) -> int32
    {
        return Z * (GridCountY * GridCountX) + Y * GridCountX + X;
    };

    auto TraceBetween = [&](const FVector &P0, const FVector &P1) -> bool
    {
        FHitResult Hit;
        const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, P0, P1, TraceChannel, QueryParams);
        return !bHit; // true if clear path
    };

    auto IsCenterFree = [&](const FVector &C) -> bool
    {
        if (!bUseCenterOverlapTest)
        {
            return true;
        }
        const float AutoR = 0.25f * FMath::Max(0.001f, FMath::Min3(CellSizeX, CellSizeY, CellSizeZ));
        const float Radius = (CenterOverlapRadius > 0.f) ? CenterOverlapRadius : AutoR;
        FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
        FHitResult Hit;
        const bool bHit = GetWorld()->SweepSingleByChannel(Hit, C, C, FQuat::Identity, TraceChannel, Shape, QueryParams);
        return !bHit;
    };

    int32 RefinedThisTick = 0;
    while (bIsRunning && RefinedThisTick < MaxCellsPerTick && CurrentHiddenIndex < HiddenBoxIndices.Num())
    {
        const int32 BoxIdx = HiddenBoxIndices[CurrentHiddenIndex];
        FS_LinkedBox &Box = PendingBoxes[BoxIdx];
        if (Box.VisibilityMask != 0)
        {
            ++CurrentHiddenIndex;
            continue; // already flipped by earlier refinement
        }

        // Build sub-voxel grid within this box's AABB
        const FBox BoxAABB = UCPP_BPL__VolumeAnalysis::LinkedBox_GetAABB(Box);
        TArray<FS_LinkedBox> SubVoxels;
        UCPP_BPL__VolumeAnalysis::GenerateVoxelGridBoxes_ByCounts(BoxAABB, SubSampleCountX, SubSampleCountY, SubSampleCountZ, SubVoxels);
        if (bDrawDebug && bDrawDebugSubBoxes)
        {
            for (const FS_LinkedBox &SV : SubVoxels)
            {
                const FBox SVBox = UCPP_BPL__VolumeAnalysis::LinkedBox_GetAABB(SV);
                DrawAABB(SVBox, FColor(0, 255, 255)); // cyan sub-box wireframes
            }
        }

        bool bAnyVisible = false;
        // Optimize refinement using long-trace row scanning along X inside the parent sample
        const auto IndexSub = [&](int32 X, int32 Y, int32 Z) -> int32
        {
            return Z * (SubSampleCountY * SubSampleCountX) + Y * SubSampleCountX + X;
        };

        auto SubCenter = [&](int32 X, int32 Y, int32 Z) -> FVector
        {
            return GetCenter(SubVoxels[IndexSub(X, Y, Z)]);
        };

        // Helper to scan a 1D row by long trace and set bAnyVisible if any free center is reachable
        auto ScanSubRow = [&](int32 Count, const TFunction<FVector(int32)> &CenterAt)
        {
            if (Count <= 0)
                return;
            if (Count == 1)
            {
                const FVector C = CenterAt(0);
                if (IsCenterFree(C))
                    bAnyVisible = true;
                return;
            }
            const FVector FirstC = CenterAt(0), SecondC = CenterAt(1);
            const float StepLen = FVector::Distance(FirstC, SecondC);
            int32 StartI = 0;
            while (StartI < Count)
            {
                int32 TargetI = Count - 1;
                if (MaxTraceDistance > 0.f && StepLen > KINDA_SMALL_NUMBER)
                {
                    const int32 MaxSteps = FMath::Clamp(static_cast<int32>(FMath::FloorToInt(MaxTraceDistance / StepLen)), 1, Count - 1);
                    TargetI = FMath::Min(StartI + MaxSteps, Count - 1);
                }
                const FVector StartC = CenterAt(StartI);
                const FVector EndC = CenterAt(TargetI);
                FHitResult Hit;
                const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, StartC, EndC, TraceChannel, QueryParams);
                if (!bHit)
                {
                    for (int32 i = StartI; i <= TargetI; ++i)
                    {
                        const FVector C = CenterAt(i);
                        if (IsCenterFree(C))
                            bAnyVisible = true;
                    }
                    if (bDrawDebug && bDrawDebugRays)
                    {
                        DrawDebugLine(GetWorld(), StartC, EndC, FColor::Cyan, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness * 0.6f);
                    }
                    StartI = TargetI + 1;
                }
                else
                {
                    const float SegmentLen = FVector::Distance(StartC, EndC);
                    const float HitDist = FMath::Clamp(Hit.Time * SegmentLen, 0.f, SegmentLen);
                    int32 HitIndex = StartI;
                    if (StepLen > KINDA_SMALL_NUMBER)
                    {
                        HitIndex = FMath::Clamp(StartI + FMath::FloorToInt(HitDist / StepLen + 1e-3f), StartI, TargetI);
                    }
                    for (int32 i = StartI; i <= HitIndex; ++i)
                    {
                        const FVector C = CenterAt(i);
                        if (IsCenterFree(C))
                            bAnyVisible = true;
                    }
                    if (bDrawDebug && bDrawDebugRays)
                    {
                        const FVector HitPoint = StartC + (EndC - StartC) * Hit.Time;
                        DrawDebugLine(GetWorld(), StartC, HitPoint, FColor::Cyan, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness * 0.6f);
                        DrawDebugLine(GetWorld(), HitPoint, EndC, FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness * 0.6f);
                    }
                    StartI = FMath::Min(HitIndex + 1, Count);
                }
            }
        };

        // X-axis rows at fixed (y,z)
        for (int32 z = 0; z < SubSampleCountZ; ++z)
        {
            for (int32 y = 0; y < SubSampleCountY; ++y)
            {
                ScanSubRow(SubSampleCountX, [&](int32 x)
                           { return SubCenter(x, y, z); });
            }
        }
        // Y-axis rows at fixed (x,z)
        for (int32 z = 0; z < SubSampleCountZ; ++z)
        {
            for (int32 x = 0; x < SubSampleCountX; ++x)
            {
                ScanSubRow(SubSampleCountY, [&](int32 y)
                           { return SubCenter(x, y, z); });
            }
        }
        // Z-axis columns at fixed (x,y)
        for (int32 y = 0; y < SubSampleCountY; ++y)
        {
            for (int32 x = 0; x < SubSampleCountX; ++x)
            {
                ScanSubRow(SubSampleCountZ, [&](int32 z)
                           { return SubCenter(x, y, z); });
            }
        }

        if (bAnyVisible)
        {
            Box.VisibilityMask = 1;
        }

        ++CurrentHiddenIndex;
        ++RefinedThisTick;
    }

    // If sub-sampling finished, finalize
    if (CurrentHiddenIndex >= HiddenBoxIndices.Num())
    {
        bIsSubSampling = false;
        bIsRunning = false;

        // Compute final counts
        VisibleCount = 0;
        HiddenCount = 0;
        for (const FS_LinkedBox &B : PendingBoxes)
        {
            if (B.VisibilityMask)
                ++VisibleCount;
            else
                ++HiddenCount;
        }
        AnalysisResults = PendingBoxes;

        if (bDrawDebug && bDrawDebugPoints)
        {
            for (const FS_LinkedBox &B : AnalysisResults)
            {
                const FVector C = UCPP_BPL__VolumeAnalysis::LinkedBox_GetCenter(B);
                DrawDebugPoint(GetWorld(), C, DebugPointSize, B.VisibilityMask ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration);
            }
        }
        UE_LOG(LogPVolActor, Display, TEXT("SubSampling: Complete; boxes=%d (Visible=%d Hidden=%d)"), AnalysisResults.Num(), VisibleCount, HiddenCount);
        OnAnalysisComplete.Broadcast(AnalysisResults);
    }
}

void ACPP_AT_VolumeAnalysis_Base::DrawAABB(const FBox &Box, const FColor &Color) const
{
    if (!GetWorld())
        return;
    const FVector C = Box.GetCenter();
    const FVector E = Box.GetExtent();
    DrawDebugBox(GetWorld(), C, E, FQuat::Identity, Color, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
}
