/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "CPP_BPL__VolumeAnalysis.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

FVector UCPP_BPL__VolumeAnalysis::GetClosestPointOnLineSegment(
	const FVector &Point,
	const FVector &LineStart,
	const FVector &LineEnd)
{
	FVector LineVec = LineEnd - LineStart;
	FVector PointVec = Point - LineStart;

	float LineLength = LineVec.Size();
	if (LineLength < KINDA_SMALL_NUMBER)
	{
		return LineStart; // Line segment is too short
	}

	FVector LineDir = LineVec / LineLength;
	float ProjectedDistance = FVector::DotProduct(PointVec, LineDir);

	// Clamp to line segment bounds
	ProjectedDistance = FMath::Clamp(ProjectedDistance, 0.0f, LineLength);

	return LineStart + (LineDir * ProjectedDistance);
}

FBox UCPP_BPL__VolumeAnalysis::MakeBoxFromPoints(const TArray<FVector> &Points)
{
	if (Points.Num() == 0)
	{
		return FBox(EForceInit::ForceInitToZero);
	}

	FBox Box(Points[0], Points[0]);
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Box += Points[i];
	}
	return Box;
}

void UCPP_BPL__VolumeAnalysis::GenerateVoxelGridBoxes_ByCounts(
	const FBox &Box,
	int32 CountX,
	int32 CountY,
	int32 CountZ,
	TArray<FS_LinkedBox> &OutBoxes)
{
	OutBoxes.Reset();
	if (!Box.IsValid || CountX <= 0 || CountY <= 0 || CountZ <= 0)
	{
		return;
	}

	const FVector Min = Box.Min;
	const FVector Max = Box.Max;

	const float StepX = (CountX > 1) ? (Max.X - Min.X) / float(CountX) : (Max.X - Min.X);
	const float StepY = (CountY > 1) ? (Max.Y - Min.Y) / float(CountY) : (Max.Y - Min.Y);
	const float StepZ = (CountZ > 1) ? (Max.Z - Min.Z) / float(CountZ) : (Max.Z - Min.Z);

	// Create CountX*CountY*CountZ boxes filling the AABB
	OutBoxes.Reserve(CountX * CountY * CountZ);
	for (int32 zi = 0; zi < CountZ; ++zi)
	{
		const float Z0 = Min.Z + zi * StepZ;
		const float Z1 = Z0 + StepZ;
		for (int32 yi = 0; yi < CountY; ++yi)
		{
			const float Y0 = Min.Y + yi * StepY;
			const float Y1 = Y0 + StepY;
			for (int32 xi = 0; xi < CountX; ++xi)
			{
				const float X0 = Min.X + xi * StepX;
				const float X1 = X0 + StepX;

				FS_LinkedBox Voxel;
				Voxel.VisibilityMask = 0;

				// Define 8 corners
				Voxel.SetBoxPoint(EE_Box_8Point::Bottom_Backward_Left, FVector(X0, Y0, Z0));
				Voxel.SetBoxPoint(EE_Box_8Point::Bottom_Backward_Right, FVector(X1, Y0, Z0));
				Voxel.SetBoxPoint(EE_Box_8Point::Bottom_Forward_Left, FVector(X0, Y1, Z0));
				Voxel.SetBoxPoint(EE_Box_8Point::Bottom_Forward_Right, FVector(X1, Y1, Z0));

				Voxel.SetBoxPoint(EE_Box_8Point::Top_Backward_Left, FVector(X0, Y0, Z1));
				Voxel.SetBoxPoint(EE_Box_8Point::Top_Backward_Right, FVector(X1, Y0, Z1));
				Voxel.SetBoxPoint(EE_Box_8Point::Top_Forward_Left, FVector(X0, Y1, Z1));
				Voxel.SetBoxPoint(EE_Box_8Point::Top_Forward_Right, FVector(X1, Y1, Z1));

				OutBoxes.Add(MoveTemp(Voxel));
			}
		}
	}
}

FVector UCPP_BPL__VolumeAnalysis::LinkedBox_GetCenter(const FS_LinkedBox &InBox)
{
	// Average available corners; if none valid, return zero
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const TPair<EE_Box_8Point, FS_LinkedSharedPoint> &Pair : InBox.Points)
	{
		if (Pair.Value.IsSharedPointValid())
		{
			Sum += Pair.Value.GetPoint();
			++Count;
		}
	}
	return (Count > 0) ? (Sum / float(Count)) : FVector::ZeroVector;
}

FVector UCPP_BPL__VolumeAnalysis::LinkedSharedPoint_GetPoint(const FS_LinkedSharedPoint &InSharedPoint)
{
	return InSharedPoint.GetPoint();
}

void UCPP_BPL__VolumeAnalysis::LinkedSharedPoint_SetPoint(FS_LinkedSharedPoint &InSharedPoint, const FVector &NewPoint)
{
	// Ensure the shared pointer exists and set the value
	FS_LinkedSharedPoint Temp = InSharedPoint;
	Temp.SetPoint(NewPoint);
	InSharedPoint = Temp;
}

bool UCPP_BPL__VolumeAnalysis::LinkedSharedPoint_IsValid(const FS_LinkedSharedPoint &InSharedPoint)
{
	return InSharedPoint.IsSharedPointValid();
}

void UCPP_BPL__VolumeAnalysis::LinkedBox_SetBoxPoint(FS_LinkedBox &InOutBox, EE_Box_8Point Corner, const FVector &NewPoint)
{
	InOutBox.SetBoxPoint(Corner, NewPoint);
}

void UCPP_BPL__VolumeAnalysis::LinkedBox_LinkTwoBoxPoint(FS_LinkedBox &BoxA, FS_LinkedBox &BoxB, EE_Box_8Point BoxA_Corner, EE_Box_8Point BoxB_Corner)
{
	FS_LinkedBox::LinkTwoBoxPoint(BoxA, BoxB, BoxA_Corner, BoxB_Corner);
}

FBox UCPP_BPL__VolumeAnalysis::LinkedBox_GetAABB(const FS_LinkedBox &InBox)
{
	TArray<FVector> Pts;
	Pts.Reserve(8);
	for (const TPair<EE_Box_8Point, FS_LinkedSharedPoint> &Pair : InBox.Points)
	{
		if (Pair.Value.IsSharedPointValid())
		{
			Pts.Add(Pair.Value.GetPoint());
		}
	}
	return MakeBoxFromPoints(Pts);
}