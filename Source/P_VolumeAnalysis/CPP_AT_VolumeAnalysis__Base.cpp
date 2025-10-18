/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "CPP_AT_VolumeAnalysis__Base.h"
#include "Components/SceneComponent.h"

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
}

/**
 * Stop the current analysis if running
 */
void ACPP_AT_VolumeAnalysis_Base::StopAnalysis()
{
}

/**
 * Clear all analysis results and visualization
 */
void ACPP_AT_VolumeAnalysis_Base::ClearResults()
{
    AnalysisResults.Reset();
    VisibleCount = 0;
    HiddenCount = 0;
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
