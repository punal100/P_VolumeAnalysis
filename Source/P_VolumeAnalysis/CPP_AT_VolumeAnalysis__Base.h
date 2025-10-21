/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CPP_BPL__VolumeAnalysis.h"
#include "CPP_AT_VolumeAnalysis__Base.generated.h"

/**
 * Delegate for broadcasting when Volume Analysis is complete
 * Allows other systems to react to finished analysis
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVolumeAnalysisComplete, const TArray<FS_LinkedBox> &, AnalysisResultBoxes);

/**
 * Main VolumeAnalysis Actor class
 */
UCLASS(BlueprintType, Blueprintable, Category = "Volume Analysis")
class P_VOLUMEANALYSIS_API ACPP_AT_VolumeAnalysis_Base : public AActor
{
    GENERATED_BODY()

public:
    /** Constructor - sets up default values and components */
    ACPP_AT_VolumeAnalysis_Base();

protected:
    /** Called when the game starts or when spawned */
    virtual void BeginPlay() override;

public:
    /** Called every frame to update analysis if needed */
    virtual void Tick(float DeltaTime) override;

    //////////////////////////////////////////////////////////////////////////
    // CORE VOLUME ANALYSIS PROPERTIES
    //////////////////////////////////////////////////////////////////////////

    /** Defines the analysis volume via its 8 linked corners (at least 2 valid points required to build an AABB) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Initial")
    FS_LinkedBox VolumeBox = FS_LinkedBox();

    /** Number of samples along X axis inside the volume */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Sampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SampleCountX = 16;

    /** Number of samples along Y axis inside the volume */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Sampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SampleCountY = 16;

    /** Number of samples along Z axis inside the volume */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Sampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SampleCountZ = 16;

    /** Trace channel used for visibility checks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Trace")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Ignore the owner actor when tracing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Trace")
    bool bIgnoreSelf = true;

    /** Max distance for line traces (0 = unlimited) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Trace", meta = (ClampMin = "0", UIMin = "0"))
    float MaxTraceDistance = 0.f;

    /** Whether to draw debug points/lines */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug")
    bool bDrawDebug = true;

    /** Draw the volume bounding box */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug")
    bool bDrawDebugBox = true;

    /** Draw trace rays from origin to sample points */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug")
    bool bDrawDebugRays = true;

    /** Draw sample points colored by visibility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug")
    bool bDrawDebugPoints = true;

    /** Draw sub-sample boxes for hidden samples during sub-sampling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug")
    bool bDrawDebugSubBoxes = false;

    /** Size of debug points */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug", meta = (ClampMin = "0", UIMin = "0"))
    float DebugPointSize = 6.f;

    /** Debug line thickness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug", meta = (ClampMin = "0", UIMin = "0"))
    float DebugLineThickness = 0.5f;

    /** Duration seconds to persist debug draw (0 = one frame) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Debug", meta = (ClampMin = "0", UIMin = "0"))
    float DebugDrawDuration = 2.0f;

    /** Number of rows to process per tick (higher = faster but may hitch) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Performance", meta = (ClampMin = "1", UIMin = "1"))
    int32 RowsPerTick = 8;

    /** Treat voxel centers overlapping blocking geometry as hidden; if true, a voxel must have a free center AND a clear neighbor trace to be visible */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Visibility")
    bool bUseCenterOverlapTest = true;

    /** Overlap radius at voxel centers; <= 0 means auto from cell size (25% of the smallest axis) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Visibility", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float CenterOverlapRadius = 0.0f;

    //////////////////////////////////////////////////////////////////////////
    // SUB-SAMPLING (refine only boxes still hidden after the main pass)
    //////////////////////////////////////////////////////////////////////////
    /** Enable a secondary sub-sampling pass that only runs for boxes remaining hidden */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|SubSampling")
    bool bEnableSubSampling = true;

    /** Sub-sample counts per axis inside each hidden box */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|SubSampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SubSampleCountX = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|SubSampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SubSampleCountY = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|SubSampling", meta = (ClampMin = "1", UIMin = "1"))
    int32 SubSampleCountZ = 2;

    //////////////////////////////////////////////////////////////////////////
    // EVENTS
    //////////////////////////////////////////////////////////////////////////

    /** Event fired when Volume analysis completes */
    UPROPERTY(BlueprintAssignable, Category = "Punal|VolumeAnalysis|Events")
    FOnVolumeAnalysisComplete OnAnalysisComplete;

    //////////////////////////////////////////////////////////////////////////
    // PUBLIC FUNCTIONS
    //////////////////////////////////////////////////////////////////////////

    /** Manually start a new volume analysis */
    UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis")
    void StartAnalysis();

    /** Stop current analysis if running */
    UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis")
    void StopAnalysis();

    /** Clear all current analysis results and visualization */
    UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis")
    void ClearResults();

    /** Get the current analysis results (flat array of voxel boxes) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punal|VolumeAnalysis")
    TArray<FS_LinkedBox> GetAnalysisResults();

    /** Get number of visible points in current analysis */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punal|VolumeAnalysis")
    int32 GetVisiblePointCount() const;

    /** Get number of hidden points in current analysis */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punal|VolumeAnalysis")
    int32 GetHiddenPointCount() const;

    /** Get visibility percentage (0-100) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punal|VolumeAnalysis")
    float GetVisibilityPercentage() const;

protected:
    //////////////////////////////////////////////////////////////////////////
    // COMPONENTS
    //////////////////////////////////////////////////////////////////////////

private:
    //////////////////////////////////////////////////////////////////////////
    // INTERNAL DATA
    //////////////////////////////////////////////////////////////////////////
    // Store analysis results (voxel boxes)
    TArray<FS_LinkedBox> AnalysisResults;
    int32 VisibleCount = 0;
    int32 HiddenCount = 0;

    // Generated voxel boxes to analyze this run (flattened Z-Y-X order)
    TArray<FS_LinkedBox> PendingBoxes;
    int32 CurrentCellIndex = 0;
    bool bIsRunning = false;

    // Cached grid extents for neighbor lookup
    int32 GridCountX = 0;
    int32 GridCountY = 0;
    int32 GridCountZ = 0;

    // Cached approximate cell sizes (computed at StartAnalysis for auto radius)
    float CellSizeX = 0.f;
    float CellSizeY = 0.f;
    float CellSizeZ = 0.f;

    // Sub-sampling phase state
    bool bIsSubSampling = false;
    TArray<int32> HiddenBoxIndices;
    int32 CurrentHiddenIndex = 0;

    // Main-pass multi-axis scan state
    // Phase 0 = X-rows (GridCountY * GridCountZ)
    // Phase 1 = Y-rows (GridCountX * GridCountZ)
    // Phase 2 = Z-columns (GridCountX * GridCountY)
    int32 CurrentPhase = 0;
    int32 CurrentPhaseRowIndex = 0;

    // Internal: process a portion of rows each tick to avoid hitching
    void ProcessRowsStep(int32 MaxRowsPerTick);
    void ProcessRowsStep_SubSampling(int32 MaxCellsPerTick, const FCollisionQueryParams &QueryParams);

    // Internal: draw an AABB
    void DrawAABB(const FBox &Box, const FColor &Color) const;

    //////////////////////////////////////////////////////////////////////////
    // INTERNAL FUNCTIONS
    //////////////////////////////////////////////////////////////////////////
};
