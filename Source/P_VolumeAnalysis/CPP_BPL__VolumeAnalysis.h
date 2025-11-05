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

UENUM(BlueprintType)
enum class EE_Box_8Point : uint8
{
	Top_Forward_Right UMETA(DisplayName = "Top Forward Right"),
	Top_Forward_Left UMETA(DisplayName = "Top Forward Left"),
	Top_Backward_Right UMETA(DisplayName = "Top Backward Right"),
	Top_Backward_Left UMETA(DisplayName = "Top Backward Left"),
	Bottom_Forward_Right UMETA(DisplayName = "Bottom Forward Right"),
	Bottom_Forward_Left UMETA(DisplayName = "Bottom Forward Left"),
	Bottom_Backward_Right UMETA(DisplayName = "Bottom Backward Right"),
	Bottom_Backward_Left UMETA(DisplayName = "Bottom Backward Left")
};

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
// Legacy point-based structs omitted; replaced by FS_LinkedBox voxel boxes.

USTRUCT(BlueprintType)
struct P_VOLUMEANALYSIS_API FS_LinkedSharedPoint
{
	GENERATED_BODY()

public:
	TSharedPtr<FVector> Point;

	FVector GetPoint() const
	{
		return Point.IsValid() ? *Point : FVector::ZeroVector;
	}

	void SetPoint(const FVector &NewPoint)
	{
		if (Point.IsValid())
		{
			*Point = NewPoint;
		}
		else
		{
			Point = MakeShared<FVector>(NewPoint);
		}
	}

	bool IsSharedPointValid() const
	{
		return Point.IsValid();
	}
};

USTRUCT(BlueprintType)
struct P_VOLUMEANALYSIS_API FS_LinkedBox
{
	GENERATED_BODY()

public:
	// Row
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|LinkedBox")
	TMap<EE_Box_8Point, FS_LinkedSharedPoint> Points;

	// Optional: Visibility mask aligned with Points_1D_Array (1 = Visible, 0 = Hidden)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punal|VolumeAnalysis|Math|LinkedBox")
	uint8 VisibilityMask;

	void SetBoxPoint(EE_Box_8Point Corner, const FVector &NewPoint)
	{
		if (Points.Contains(Corner))
		{
			Points[Corner].SetPoint(NewPoint);
		}
		else
		{
			FS_LinkedSharedPoint NewSharedPoint;
			NewSharedPoint.SetPoint(NewPoint);
			Points.Add(Corner, NewSharedPoint);
		}
	}

	static void LinkTwoBoxPoint(FS_LinkedBox &BoxA, FS_LinkedBox &BoxB, EE_Box_8Point BoxA_Corner, EE_Box_8Point BoxB_Corner)
	{
		if (BoxA.Points.Contains(BoxA_Corner) && BoxB.Points.Contains(BoxB_Corner))
		{
			BoxA.Points[BoxA_Corner].Point = BoxB.Points[BoxB_Corner].Point;
		}
	}
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

	// Generate a voxel grid of boxes within the AABB using counts per axis
	static void GenerateVoxelGridBoxes_ByCounts(
		const FBox &Box,
		int32 CountX,
		int32 CountY,
		int32 CountZ,
		TArray<FS_LinkedBox> &OutBoxes);

	// Utility: Get center of a linked box (averaging available corners; falls back to AABB center of valid points)
	UFUNCTION(BlueprintPure, Category = "Punal|VolumeAnalysis|LinkedBox")
	static FVector LinkedBox_GetCenter(const FS_LinkedBox &InBox);

	// Utility: Compute AABB from a linked box's valid corners
	UFUNCTION(BlueprintPure, Category = "Punal|VolumeAnalysis|LinkedBox")
	static FBox LinkedBox_GetAABB(const FS_LinkedBox &InBox);

	// Utility: FS_LinkedSharedPoint Blueprint wrappers
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedSharedPoint")
	static FVector LinkedSharedPoint_GetPoint(const FS_LinkedSharedPoint &InSharedPoint);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedSharedPoint")
	static void LinkedSharedPoint_SetPoint(UPARAM(ref) FS_LinkedSharedPoint &InSharedPoint, const FVector &NewPoint);

	UFUNCTION(BlueprintPure, Category = "Punal|VolumeAnalysis|LinkedSharedPoint")
	static bool LinkedSharedPoint_IsValid(const FS_LinkedSharedPoint &InSharedPoint);

	// Utility: FS_LinkedBox Blueprint wrappers
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox")
	static void LinkedBox_SetBoxPoint(UPARAM(ref) FS_LinkedBox &InOutBox, EE_Box_8Point Corner, const FVector &NewPoint);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox")
	static void LinkedBox_LinkTwoBoxPoint(UPARAM(ref) FS_LinkedBox &BoxA, UPARAM(ref) FS_LinkedBox &BoxB, EE_Box_8Point BoxA_Corner, EE_Box_8Point BoxB_Corner);

	// JSON Serialization helpers for FS_LinkedBox
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LinkedBox_ToJsonString(const FS_LinkedBox &InBox, FString &OutJson, bool bPretty = true);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool SaveLinkedBoxToJsonFile(const FS_LinkedBox &InBox, const FString &FilePath, bool bPretty = true);

	// Serialize an array of boxes (e.g., full analysis results) to a JSON array string / file
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LinkedBoxes_ToJsonString(const TArray<FS_LinkedBox> &InBoxes, FString &OutJson, bool bPretty = true);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool SaveLinkedBoxesToJsonFile(const TArray<FS_LinkedBox> &InBoxes, const FString &FilePath, bool bPretty = true);

	// JSON Deserialization helpers
	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LinkedBox_FromJsonString(const FString &InJson, FS_LinkedBox &OutBox);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LoadLinkedBoxFromJsonFile(const FString &FilePath, FS_LinkedBox &OutBox);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LinkedBoxes_FromJsonString(const FString &InJson, TArray<FS_LinkedBox> &OutBoxes);

	UFUNCTION(BlueprintCallable, Category = "Punal|VolumeAnalysis|LinkedBox|JSON")
	static bool LoadLinkedBoxesFromJsonFile(const FString &FilePath, TArray<FS_LinkedBox> &OutBoxes);
};
