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
        bIsRunning = false;
        return;
    }

    // Compute axis-aligned bounding box from provided points
    const FBox Box = UCPP_BPL__VolumeAnalysis::MakeBoxFromPoints(VolumePoints.Points_1D_Array);
    if (!Box.IsValid)
    {
        bIsRunning = false;
        return;
    }

    // Generate grid rows within the box
    PendingRows.Reset();
    UCPP_BPL__VolumeAnalysis::GenerateGridRowsInBox_ByCounts(Box, SampleCountX, SampleCountY, SampleCountZ, PendingRows);
    CurrentRowIndex = 0;
    bIsRunning = PendingRows.Num() > 0;

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

TArray<FS_V3_1D__Array> ACPP_AT_VolumeAnalysis_Base::GetAnalysisResults()
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
        bIsRunning = false;
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VolumeAnalysis), /*bTraceComplex*/ true);
    if (bIgnoreSelf)
    {
        QueryParams.AddIgnoredActor(this);
    }

    const FVector Origin = GetActorLocation();

    int32 RowsProcessed = 0;
    while (bIsRunning && RowsProcessed < MaxRowsPerTick && CurrentRowIndex < PendingRows.Num())
    {
        FS_V3_1D__Array Row = PendingRows[CurrentRowIndex];
        UCPP_BPL__VolumeAnalysis::EnsureRowMaskSize(Row);

        for (int32 i = 0; i < Row.Points_1D_Array.Num(); ++i)
        {
            const FVector Point = Row.Points_1D_Array[i];
            FVector Start = Origin;
            FVector End = Point;

            if (MaxTraceDistance > 0.f)
            {
                const FVector Dir = (Point - Origin).GetSafeNormal();
                const float DistToPoint = FVector::Dist(Origin, Point);
                const float ClampDist = FMath::Min(MaxTraceDistance, DistToPoint);
                End = Origin + Dir * ClampDist;
            }

            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, QueryParams);

            const bool bVisible = !bHit || (Hit.bBlockingHit && Hit.GetActor() == this);
            Row.VisibilityMask[i] = bVisible ? 1 : 0;
            if (bVisible)
            {
                ++VisibleCount;
            }
            else
            {
                ++HiddenCount;
            }

            if (bDrawDebug)
            {
                const FColor PColor = bVisible ? FColor::Green : FColor::Red;
                if (bDrawDebugPoints)
                {
                    DrawDebugPoint(GetWorld(), Point, DebugPointSize, PColor, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration);
                }
                if (bDrawDebugRays)
                {
                    DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End, PColor, /*bPersistentLines*/ DebugDrawDuration > 0.f, DebugDrawDuration, 0, DebugLineThickness);
                }
            }
        }

        AnalysisResults.Add(MoveTemp(Row));
        ++CurrentRowIndex;
        ++RowsProcessed;
    }

    // If done, broadcast and stop
    if (CurrentRowIndex >= PendingRows.Num())
    {
        bIsRunning = false;
        // Build a single combined result to broadcast
        FS_V3_1D__Array Combined;
        int32 TotalPts = 0;
        for (const FS_V3_1D__Array &Row : AnalysisResults)
        {
            TotalPts += Row.Points_1D_Array.Num();
        }
        Combined.Points_1D_Array.Reserve(TotalPts);
        Combined.VisibilityMask.Reserve(TotalPts);
        for (const FS_V3_1D__Array &Row : AnalysisResults)
        {
            Combined.Points_1D_Array.Append(Row.Points_1D_Array);
            Combined.VisibilityMask.Append(Row.VisibilityMask);
        }
        UE_LOG(LogPVolActor, Display, TEXT("P_VolumeAnalysis: Broadcasting OnAnalysisComplete with %d points"), Combined.Points_1D_Array.Num());
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
