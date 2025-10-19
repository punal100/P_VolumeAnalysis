/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "CPP_BPL__VolumeAnalysis.generated.h"

// Punal Manalan, NOTE: Not Required for For Volume Analysis but can be used in Future Extensions
// USTRUCT(BlueprintType)
// struct P_VOLUMEANALYSIS_API FS_V3_1D__Array
// {
// 		GENERATED_BODY()
//
// public:
// 	// Row
//		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point|Array")
//		TArray<FVector> Points_1D_Array;
// };
//  USTRUCT(BlueprintType)
//  struct P_VOLUMEANALYSIS_API FS_V3_2D__Array
//  {
//  	GENERATED_BODY()
//  public:
//  	// Column
//  	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point|Array")
//  	TArray<FS_V3_1D__Array> Points_2D_Array;
//  };
//
//  USTRUCT(BlueprintType)
//  struct P_VOLUMEANALYSIS_API FS_V3_3D__Array
//  {
//  	GENERATED_BODY()
//  public:
//  	// Layer or Slice
//  	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point|Array")
//  	TArray<FS_V3_2D__Array> Points_3D_Array;
//  };

USTRUCT(BlueprintType)
struct P_VOLUMEANALYSIS_API FS_VolumeAnalysis_Point
{
	GENERATED_BODY()

public:
	// Row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point")
	FVector Points_1D_Array;

	// Optional: Visibility mask aligned with Points_1D_Array (1 = Visible, 0 = Hidden)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point")
	uint8 VisibilityMask;
};

USTRUCT(BlueprintType)
struct P_VOLUMEANALYSIS_API FS_VolumeAnalysis_Point__Array
{
	GENERATED_BODY()

public:
	// Row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point|Array")
	TArray<FS_VolumeAnalysis_Point> Points_1D_Array;
};

/**
 *
 */
UCLASS()
class P_VOLUMEANALYSIS_API UCPP_BPL__VolumeAnalysis : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Helper functions
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|Math|LineSegment")
	static FVector GetClosestPointOnLineSegment(
		const FVector &Point,
		const FVector &LineStart,
		const FVector &LineEnd);

	// Compute axis-aligned bounding box from a set of points
	static FBox MakeBoxFromPoints(const TArray<FVector> &Points);

	// Generate 3D grid rows across X for each Y,Z within the given box using counts per axis
	static void GenerateGridRowsInBox_ByCounts(
		const FBox &Box,
		int32 CountX,
		int32 CountY,
		int32 CountZ,
		TArray<FS_VolumeAnalysis_Point__Array> &OutRows);

	// Utility: Ensure a row's points have initialized visibility masks
	static void EnsureRowMaskSize(FS_VolumeAnalysis_Point__Array &Row);
};
