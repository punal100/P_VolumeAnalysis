/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "CPP_AT_VolumeAnalysis__Base.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVolActor, Log, All);

/**
 * Constructor - Initialize default values and create components
 */
ACPP_AT_VolumeAnalysis_Base::ACPP_AT_VolumeAnalysis_Base()
{
    // Enable tick for this actor so we can update analysis over time
    PrimaryActorTick.bCanEverTick = true;

    // Create the root scene component to anchor everything
    USceneComponent *RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
    RootComponent = RootComp;
}

/**
 * Called when the actor begins play
 */
void ACPP_AT_VolumeAnalysis_Base::BeginPlay()
{
    // Call parent implementation
    Super::BeginPlay();
}

/**
 * Called every frame to update analysis progress and handle auto-updates
 */
void ACPP_AT_VolumeAnalysis_Base::Tick(float DeltaTime)
{
    // Call parent implementation first
    Super::Tick(DeltaTime);

    if (bIsRunning)
    {
        // Process a few rows per tick to spread work (tune as needed)
        ProcessRowsStep(RowsPerTick);
    }
}

/**
 * Start a new Volume analysis
 * Generates trace endpoints and begins processing
 */
void ACPP_AT_VolumeAnalysis_Base::StartAnalysis()
{
    // Reset counts and results
    AnalysisResults.Reset();
    VisibleCount = 0;
    HiddenCount = 0;

    // Validate input linked box: gather any valid points
    TArray<FVector> TempPts;
    TempPts.Reserve(8);
    for (const TPair<EE_Box_8Point, FS_LinkedSharedPoint> &Pair : VolumeBox.Points)
    {
        if (Pair.Value.IsSharedPointValid())
        {
            TempPts.Add(Pair.Value.GetPoint());
        }
    }
    if (TempPts.Num() < 2)
    {
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: VolumeBox has insufficient valid points: %d"), TempPts.Num());
        bIsRunning = false;
        return;
    }

    // Compute axis-aligned bounding box from provided points
    const FBox Box = UCPP_BPL__VolumeAnalysis::MakeBoxFromPoints(TempPts);
    if (!Box.IsValid)
    {
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: Computed Box is invalid"));
        bIsRunning = false;
        return;
    }

    // Generate voxel boxes within the box
    PendingBoxes.Reset();
    GridCountX = SampleCountX;
    GridCountY = SampleCountY;
    GridCountZ = SampleCountZ;
    UCPP_BPL__VolumeAnalysis::GenerateVoxelGridBoxes_ByCounts(Box, GridCountX, GridCountY, GridCountZ, PendingBoxes);
    // Cache cell sizes for overlap radius auto scaling
    const FVector Extent = Box.GetExtent();
    CellSizeX = (GridCountX > 0) ? (Extent.X * 2.f / GridCountX) : 0.f;
    CellSizeY = (GridCountY > 0) ? (Extent.Y * 2.f / GridCountY) : 0.f;
    CellSizeZ = (GridCountZ > 0) ? (Extent.Z * 2.f / GridCountZ) : 0.f;
    CurrentCellIndex = 0;
    bIsSubSampling = false;
    HiddenBoxIndices.Reset();
    CurrentHiddenIndex = 0;
    bIsRunning = PendingBoxes.Num() > 0;
    UE_LOG(LogPVolActor, Display, TEXT("StartAnalysis: Generated %d voxel boxes; Running=%s"), PendingBoxes.Num(), bIsRunning ? TEXT("true") : TEXT("false"));

    if (bDrawDebug && bDrawDebugBox)
    {
        DrawAABB(Box, FColor::Yellow);
    }
}

/**
 * Stop the current analysis if running
 */
void ACPP_AT_VolumeAnalysis_Base::StopAnalysis()
{
    bIsRunning = false;
}

/**
 * Clear all analysis results and visualization
 */
void ACPP_AT_VolumeAnalysis_Base::ClearResults()
{
    AnalysisResults.Reset();
    VisibleCount = 0;
    HiddenCount = 0;
    PendingBoxes.Reset();
    CurrentCellIndex = 0;
    bIsSubSampling = false;
    HiddenBoxIndices.Reset();
    CurrentHiddenIndex = 0;
    bIsRunning = false;
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

    int32 CellsProcessed = 0;
    while (bIsRunning && CellsProcessed < MaxRowsPerTick && CurrentCellIndex < PendingBoxes.Num())
    {
        const int32 ZIndex = CurrentCellIndex / (GridCountY * GridCountX);
        const int32 Rem = CurrentCellIndex % (GridCountY * GridCountX);
        const int32 YIndex = Rem / GridCountX;
        const int32 XIndex = Rem % GridCountX;

        FS_LinkedBox &ABox = PendingBoxes[CurrentCellIndex];
        const FVector AC = GetCenter(ABox);

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
            // SphereOverlap returns actors; we can use a collision query via Sweep with a small sphere
            FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
            FHitResult Hit;
            const bool bHit = GetWorld()->SweepSingleByChannel(Hit, C, C, FQuat::Identity, TraceChannel, Shape, QueryParams);
            return !bHit;
        };

        // +X neighbor
        if (XIndex + 1 < GridCountX)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex + 1, YIndex, ZIndex)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        // -X neighbor
        if (XIndex - 1 >= 0)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex - 1, YIndex, ZIndex)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        // +Y neighbor
        if (YIndex + 1 < GridCountY)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex, YIndex + 1, ZIndex)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        // -Y neighbor
        if (YIndex - 1 >= 0)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex, YIndex - 1, ZIndex)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        // +Z neighbor
        if (ZIndex + 1 < GridCountZ)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex, YIndex, ZIndex + 1)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        // -Z neighbor
        if (ZIndex - 1 >= 0)
        {
            FS_LinkedBox &BBox = PendingBoxes[Index(XIndex, YIndex, ZIndex - 1)];
            const FVector BC = GetCenter(BBox);
            const bool bClear = TraceBetween(AC, BC);
            if (bClear && IsCenterFree(AC) && IsCenterFree(BC))
            {
                ABox.VisibilityMask = 1;
                BBox.VisibilityMask = 1;
            }
            if (bDrawDebug && bDrawDebugRays)
            {
                DrawDebugLine(GetWorld(), AC, BC, bClear ? FColor::Green : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
            }
        }

        ++CurrentCellIndex;
        ++CellsProcessed;
    }

    // If done, broadcast and stop
    if (bIsRunning && CurrentCellIndex < PendingBoxes.Num())
    {
        UE_LOG(LogPVolActor, VeryVerbose, TEXT("ProcessRowsStep: Progress Cell=%d/%d"), CurrentCellIndex, PendingBoxes.Num());
    }

    if (CurrentCellIndex >= PendingBoxes.Num())
    {
        // Main pass completed; optionally start sub-sampling for hidden boxes
        if (bEnableSubSampling && !bIsSubSampling)
        {
            HiddenBoxIndices.Reset();
            HiddenBoxIndices.Reserve(PendingBoxes.Num());
            int32 TmpVisible = 0;
            int32 TmpHidden = 0;
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
            TmpHidden = HiddenBoxIndices.Num();
            bIsSubSampling = HiddenBoxIndices.Num() > 0;
            CurrentHiddenIndex = 0;
            if (bIsSubSampling)
            {
                // Ensure we remain running for sub-sampling
                bIsRunning = true;
                CurrentCellIndex = PendingBoxes.Num();
                UE_LOG(LogPVolActor, Display, TEXT("SubSampling: %d hidden boxes to refine (Visible after main=%d, Hidden=%d)"), HiddenBoxIndices.Num(), TmpVisible, TmpHidden);
                // proceed immediately into sub-sampling this tick using remaining budget
                const int32 Remaining = MaxRowsPerTick - CellsProcessed;
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

        // If no sub-sampling or none to refine, finalize
        if (!bIsSubSampling)
        {
            bIsRunning = false;
            // Compute final counts from the grid
            VisibleCount = 0;
            HiddenCount = 0;
            for (const FS_LinkedBox &Box : PendingBoxes)
            {
                if (Box.VisibilityMask)
                {
                    ++VisibleCount;
                }
                else
                {
                    ++HiddenCount;
                }
            }

            // Persist results
            AnalysisResults = PendingBoxes;

            // Optional: draw centers once with their final visibility
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

    // If main pass still running or we have transitioned into sub-sampling, continue sub-sampling if needed
    if (bIsSubSampling)
    {
        const int32 Remaining = MaxRowsPerTick - CellsProcessed;
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
        // Refine using internal sub-voxel neighbor connectivity inside the parent sample
        const auto IndexSub = [&](int32 X, int32 Y, int32 Z) -> int32
        {
            return Z * (SubSampleCountY * SubSampleCountX) + Y * SubSampleCountX + X;
        };

        for (int32 z = 0; z < SubSampleCountZ; ++z)
        {
            for (int32 y = 0; y < SubSampleCountY; ++y)
            {
                for (int32 x = 0; x < SubSampleCountX; ++x)
                {
                    const FS_LinkedBox &Sub = SubVoxels[IndexSub(x, y, z)];
                    const FVector SC = GetCenter(Sub);

                    auto CheckNeighbor = [&](int32 nx, int32 ny, int32 nz)
                    {
                        const FS_LinkedBox &NSub = SubVoxels[IndexSub(nx, ny, nz)];
                        const FVector NC = GetCenter(NSub);
                        const bool bClear = TraceBetween(SC, NC);
                        if (bDrawDebug && bDrawDebugRays)
                        {
                            DrawDebugLine(GetWorld(), SC, NC, bClear ? FColor::Cyan : FColor::Red, DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness * 0.6f);
                        }
                        if (bClear && IsCenterFree(SC) && IsCenterFree(NC))
                        {
                            bAnyVisible = true;
                        }
                    };

                    // 6-neighborhood (±X, ±Y, ±Z) inside the parent sample
                    if (x + 1 < SubSampleCountX)
                        CheckNeighbor(x + 1, y, z);
                    if (x - 1 >= 0)
                        CheckNeighbor(x - 1, y, z);
                    if (y + 1 < SubSampleCountY)
                        CheckNeighbor(x, y + 1, z);
                    if (y - 1 >= 0)
                        CheckNeighbor(x, y - 1, z);
                    if (z + 1 < SubSampleCountZ)
                        CheckNeighbor(x, y, z + 1);
                    if (z - 1 >= 0)
                        CheckNeighbor(x, y, z - 1);
                }
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
