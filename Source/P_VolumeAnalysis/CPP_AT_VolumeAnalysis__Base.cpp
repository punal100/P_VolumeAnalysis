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

    // Validate input points
    if (VolumePoints.Points_1D_Array.Num() < 2)
    {
        // Need at least two points to compute a valid box
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: Not enough input points: %d"), VolumePoints.Points_1D_Array.Num());
        bIsRunning = false;
        return;
    }

    // Compute axis-aligned bounding box from provided points
    TArray<FVector> TempPts;
    TempPts.Reserve(VolumePoints.Points_1D_Array.Num());
    for (const FS_VolumeAnalysis_Point &P : VolumePoints.Points_1D_Array)
    {
        TempPts.Add(P.Points_1D_Array);
    }
    const FBox Box = UCPP_BPL__VolumeAnalysis::MakeBoxFromPoints(TempPts);
    if (!Box.IsValid)
    {
        UE_LOG(LogPVolActor, Warning, TEXT("StartAnalysis: Computed Box is invalid"));
        bIsRunning = false;
        return;
    }

    // Generate grid rows within the box
    PendingRows.Reset();
    UCPP_BPL__VolumeAnalysis::GenerateGridRowsInBox_ByCounts(Box, SampleCountX, SampleCountY, SampleCountZ, PendingRows);
    CurrentRowIndex = 0;
    bIsRunning = PendingRows.Num() > 0;
    UE_LOG(LogPVolActor, Display, TEXT("StartAnalysis: Generated %d rows; Running=%s"), PendingRows.Num(), bIsRunning ? TEXT("true") : TEXT("false"));

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
    PendingRows.Reset();
    CurrentRowIndex = 0;
    bIsRunning = false;
}

TArray<FS_VolumeAnalysis_Point__Array> ACPP_AT_VolumeAnalysis_Base::GetAnalysisResults()
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

    int32 RowsProcessed = 0;
    while (bIsRunning && RowsProcessed < MaxRowsPerTick && CurrentRowIndex < PendingRows.Num())
    {
        // Work on the row by reference so neighbor updates persist in PendingRows
        FS_VolumeAnalysis_Point__Array &Row = PendingRows[CurrentRowIndex];
        UCPP_BPL__VolumeAnalysis::EnsureRowMaskSize(Row);

        // Compute this row's Y/Z indices in the grid
        const int32 ZIndex = CurrentRowIndex / SampleCountY;
        const int32 YIndex = CurrentRowIndex % SampleCountY;

        for (int32 xi = 0; xi < Row.Points_1D_Array.Num(); ++xi)
        {
            // Current point
            FS_VolumeAnalysis_Point &A = Row.Points_1D_Array[xi];

            auto TraceBetween = [&](const FVector &P0, const FVector &P1) -> bool
            {
                FHitResult Hit;
                const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, P0, P1, TraceChannel, QueryParams);
                return !bHit; // true if clear path
            };

            // +X neighbor within the same row
            if (xi + 1 < SampleCountX)
            {
                FS_VolumeAnalysis_Point &B = Row.Points_1D_Array[xi + 1];
                const bool bClear = TraceBetween(A.Points_1D_Array, B.Points_1D_Array);
                if (bClear)
                {
                    A.VisibilityMask = 1;
                    B.VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), A.Points_1D_Array, B.Points_1D_Array, bClear ? FColor::Green : FColor::Red, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
            }

            // +Y neighbor: next row in the same Z slice
            if (YIndex + 1 < SampleCountY)
            {
                FS_VolumeAnalysis_Point__Array &RowY = PendingRows[CurrentRowIndex + 1];
                FS_VolumeAnalysis_Point &B = RowY.Points_1D_Array[xi];
                const bool bClear = TraceBetween(A.Points_1D_Array, B.Points_1D_Array);
                if (bClear)
                {
                    A.VisibilityMask = 1;
                    B.VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), A.Points_1D_Array, B.Points_1D_Array, bClear ? FColor::Green : FColor::Red, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
            }

            // +Z neighbor: corresponding row in the next Z slice
            if (ZIndex + 1 < SampleCountZ)
            {
                const int32 NextZRowIndex = CurrentRowIndex + SampleCountY;
                FS_VolumeAnalysis_Point__Array &RowZ = PendingRows[NextZRowIndex];
                FS_VolumeAnalysis_Point &B = RowZ.Points_1D_Array[xi];
                const bool bClear = TraceBetween(A.Points_1D_Array, B.Points_1D_Array);
                if (bClear)
                {
                    A.VisibilityMask = 1;
                    B.VisibilityMask = 1;
                }
                if (bDrawDebug && bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), A.Points_1D_Array, B.Points_1D_Array, bClear ? FColor::Green : FColor::Red, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
            }
        }
        ++CurrentRowIndex;
        ++RowsProcessed;
    }

    // If done, broadcast and stop
    if (bIsRunning && CurrentRowIndex < PendingRows.Num())
    {
        UE_LOG(LogPVolActor, VeryVerbose, TEXT("ProcessRowsStep: Progress Row=%d/%d"), CurrentRowIndex, PendingRows.Num());
    }

    if (CurrentRowIndex >= PendingRows.Num())
    {
        bIsRunning = false;
        // Compute final counts from the grid
        VisibleCount = 0;
        HiddenCount = 0;
        for (const FS_VolumeAnalysis_Point__Array &R : PendingRows)
        {
            for (const FS_VolumeAnalysis_Point &P : R.Points_1D_Array)
            {
                if (P.VisibilityMask)
                {
                    ++VisibleCount;
                }
                else
                {
                    ++HiddenCount;
                }
            }
        }

        // Persist results and build a combined array
        AnalysisResults = PendingRows;
        FS_VolumeAnalysis_Point__Array Combined;
        int32 TotalPts = 0;
        for (const FS_VolumeAnalysis_Point__Array &R : AnalysisResults)
        {
            TotalPts += R.Points_1D_Array.Num();
        }
        Combined.Points_1D_Array.Reserve(TotalPts);
        for (const FS_VolumeAnalysis_Point__Array &R : AnalysisResults)
        {
            Combined.Points_1D_Array.Append(R.Points_1D_Array);
        }

        // Optional: draw points once with their final visibility
        if (bDrawDebug && bDrawDebugPoints)
        {
            for (const FS_VolumeAnalysis_Point &P : Combined.Points_1D_Array)
            {
                DrawDebugPoint(GetWorld(), P.Points_1D_Array, DebugPointSize, P.VisibilityMask ? FColor::Green : FColor::Red, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration);
            }
        }

        UE_LOG(LogPVolActor, Display, TEXT("ProcessRowsStep: Complete; points=%d (Visible=%d Hidden=%d)"), Combined.Points_1D_Array.Num(), VisibleCount, HiddenCount);
        OnAnalysisComplete.Broadcast(Combined);
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
