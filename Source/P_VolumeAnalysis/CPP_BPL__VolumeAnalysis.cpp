/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "CPP_BPL__VolumeAnalysis.h"
#include "Kismet/KismetSystemLibrary.h"

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