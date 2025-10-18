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

USTRUCT(BlueprintType)
struct P_VOLUMEANALYSIS_API FS_V3_1D__Array
{
	GENERATED_BODY()

public:
	// Row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|Point|Array")
	TArray<FVector> Points_1D_Array;
};

// Punal Manalan, NOTE: Not Required for For Volume Analysis but can be used in Future Extensions
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
};
