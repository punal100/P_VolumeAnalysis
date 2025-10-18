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

void UCPP_BPL__VolumeAnalysis::GenerateGridRowsInBox_ByCounts(
	const FBox &Box,
	int32 CountX,
	int32 CountY,
	int32 CountZ,
	TArray<FS_V3_1D__Array> &OutRows)
{
	OutRows.Reset();
	if (!Box.IsValid || CountX <= 0 || CountY <= 0 || CountZ <= 0)
	{
		return;
	}

	const FVector Min = Box.Min;
	const FVector Max = Box.Max;

	const float StepX = (CountX > 1) ? (Max.X - Min.X) / float(CountX - 1) : 0.f;
	const float StepY = (CountY > 1) ? (Max.Y - Min.Y) / float(CountY - 1) : 0.f;
	const float StepZ = (CountZ > 1) ? (Max.Z - Min.Z) / float(CountZ - 1) : 0.f;

	for (int32 zi = 0; zi < CountZ; ++zi)
	{
		const float Z = Min.Z + zi * StepZ;
		for (int32 yi = 0; yi < CountY; ++yi)
		{
			const float Y = Min.Y + yi * StepY;

			FS_V3_1D__Array Row;
			Row.Points_1D_Array.Reserve(CountX);
			for (int32 xi = 0; xi < CountX; ++xi)
			{
				const float X = Min.X + xi * StepX;
				Row.Points_1D_Array.Emplace(X, Y, Z);
			}
			EnsureRowMaskSize(Row);
			OutRows.Add(MoveTemp(Row));
		}
	}
}

void UCPP_BPL__VolumeAnalysis::EnsureRowMaskSize(FS_V3_1D__Array &Row)
{
	const int32 N = Row.Points_1D_Array.Num();
	Row.VisibilityMask.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		Row.VisibilityMask[i] = 0; // default hidden
	}
}