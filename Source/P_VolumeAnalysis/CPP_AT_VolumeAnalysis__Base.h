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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVolumeAnalysisComplete, const FS_V3_1D__Array &, AnalysisResultPoint);

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

    /** Contains a Minimum of 8 Points, Making a Cube Shape */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Initial")
    FS_V3_1D__Array VolumePoints = FS_V3_1D__Array();

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

    /** Get the current analysis results */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Punal|VolumeAnalysis")
    TArray<FS_V3_1D__Array> GetAnalysisResults();

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
    // Store analysis results
    TArray<FS_V3_1D__Array> AnalysisResults;
    int32 VisibleCount = 0;
    int32 HiddenCount = 0;

    // Generated rows to analyze this run
    TArray<FS_V3_1D__Array> PendingRows;
    int32 CurrentRowIndex = 0;
    bool bIsRunning = false;

    // Internal: process a portion of rows each tick to avoid hitching
    void ProcessRowsStep(int32 MaxRowsPerTick);

    // Internal: draw an AABB
    void DrawAABB(const FBox &Box, const FColor &Color) const;

    //////////////////////////////////////////////////////////////////////////
    // INTERNAL FUNCTIONS
    //////////////////////////////////////////////////////////////////////////
};
