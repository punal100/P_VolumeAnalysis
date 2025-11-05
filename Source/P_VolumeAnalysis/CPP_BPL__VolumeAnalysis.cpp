/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "CPP_BPL__VolumeAnalysis.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

// --- JSON Serialization ---
static FString EnumToString(EE_Box_8Point Corner)
{
	if (const UEnum *Enum = StaticEnum<EE_Box_8Point>())
	{
		return Enum->GetNameStringByValue(static_cast<int64>(Corner));
	}
	return TEXT("");
}

static TSharedPtr<FJsonObject> MakeJsonFromVector(const FVector &V)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("X"), V.X);
	Obj->SetNumberField(TEXT("Y"), V.Y);
	Obj->SetNumberField(TEXT("Z"), V.Z);
	return Obj;
}

static TSharedPtr<FJsonObject> MakeJsonFromLinkedBox(const FS_LinkedBox &InBox)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("VisibilityMask"), static_cast<double>(InBox.VisibilityMask));

	// Points as object mapping enum-name -> {X,Y,Z}
	TSharedPtr<FJsonObject> PointsObj = MakeShared<FJsonObject>();
	for (const TPair<EE_Box_8Point, FS_LinkedSharedPoint> &Pair : InBox.Points)
	{
		if (Pair.Value.IsSharedPointValid())
		{
			const FString Key = EnumToString(Pair.Key);
			const FVector P = Pair.Value.GetPoint();
			PointsObj->SetObjectField(Key, MakeJsonFromVector(P));
		}
	}
	Root->SetObjectField(TEXT("Points"), PointsObj);
	return Root;
}

bool UCPP_BPL__VolumeAnalysis::LinkedBox_ToJsonString(const FS_LinkedBox &InBox, FString &OutJson, bool bPretty)
{
	OutJson.Reset();
	const TSharedPtr<FJsonObject> Root = MakeJsonFromLinkedBox(InBox);
	if (!Root.IsValid())
	{
		return false;
	}
	bool bOk = false;
	if (bPretty)
	{
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		bOk = FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		bOk = FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	}
	return bOk;
}

bool UCPP_BPL__VolumeAnalysis::SaveLinkedBoxToJsonFile(const FS_LinkedBox &InBox, const FString &FilePath, bool bPretty)
{
	FString Json;
	if (!LinkedBox_ToJsonString(InBox, Json, bPretty))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Json, *FilePath);
}

bool UCPP_BPL__VolumeAnalysis::LinkedBoxes_ToJsonString(const TArray<FS_LinkedBox> &InBoxes, FString &OutJson, bool bPretty)
{
	OutJson.Reset();
	TArray<TSharedPtr<FJsonValue>> Items;
	Items.Reserve(InBoxes.Num());
	for (const FS_LinkedBox &Box : InBoxes)
	{
		TSharedPtr<FJsonObject> Obj = MakeJsonFromLinkedBox(Box);
		Items.Add(MakeShared<FJsonValueObject>(Obj));
	}
	// Serialize as a JSON array
	bool bOk = false;
	if (bPretty)
	{
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		bOk = FJsonSerializer::Serialize(Items, Writer);
	}
	else
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutJson);
		bOk = FJsonSerializer::Serialize(Items, Writer);
	}
	return bOk;
}

bool UCPP_BPL__VolumeAnalysis::SaveLinkedBoxesToJsonFile(const TArray<FS_LinkedBox> &InBoxes, const FString &FilePath, bool bPretty)
{
	FString Json;
	if (!LinkedBoxes_ToJsonString(InBoxes, Json, bPretty))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Json, *FilePath);
}

// --- JSON Deserialization ---
static bool StringToEnum(const FString &Name, EE_Box_8Point &OutCorner)
{
	if (const UEnum *Enum = StaticEnum<EE_Box_8Point>())
	{
		int64 Value = Enum->GetValueByNameString(Name);
		if (Value != INDEX_NONE)
		{
			OutCorner = static_cast<EE_Box_8Point>(Value);
			return true;
		}
	}
	return false;
}

static bool JsonToVector(const TSharedPtr<FJsonObject> &Obj, FVector &Out)
{
	if (!Obj.IsValid())
		return false;
	double X = 0, Y = 0, Z = 0;
	if (!Obj->TryGetNumberField(TEXT("X"), X))
		return false;
	if (!Obj->TryGetNumberField(TEXT("Y"), Y))
		return false;
	if (!Obj->TryGetNumberField(TEXT("Z"), Z))
		return false;
	Out = FVector((float)X, (float)Y, (float)Z);
	return true;
}

static bool JsonObjectToLinkedBox(const TSharedPtr<FJsonObject> &Root, FS_LinkedBox &OutBox)
{
	if (!Root.IsValid())
		return false;
	// Visibility
	double VM = 0.0;
	if (Root->TryGetNumberField(TEXT("VisibilityMask"), VM))
	{
		OutBox.VisibilityMask = static_cast<uint8>((int32)VM & 0xFF);
	}
	else
	{
		OutBox.VisibilityMask = 0;
	}
	// Points
	TSharedPtr<FJsonObject> PointsObj = Root->GetObjectField(TEXT("Points"));
	if (PointsObj.IsValid())
	{
		for (const auto &Pair : PointsObj->Values)
		{
			EE_Box_8Point Corner;
			if (!StringToEnum(Pair.Key, Corner))
			{
				continue; // skip unknown keys
			}
			TSharedPtr<FJsonObject> VecObj = Pair.Value->AsObject();
			FVector P;
			if (JsonToVector(VecObj, P))
			{
				OutBox.SetBoxPoint(Corner, P);
			}
		}
	}
	return true;
}

bool UCPP_BPL__VolumeAnalysis::LinkedBox_FromJsonString(const FString &InJson, FS_LinkedBox &OutBox)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	OutBox = FS_LinkedBox();
	return JsonObjectToLinkedBox(Root, OutBox);
}

bool UCPP_BPL__VolumeAnalysis::LoadLinkedBoxFromJsonFile(const FString &FilePath, FS_LinkedBox &OutBox)
{
	FString Data;
	if (!FFileHelper::LoadFileToString(Data, *FilePath))
	{
		return false;
	}
	return LinkedBox_FromJsonString(Data, OutBox);
}

bool UCPP_BPL__VolumeAnalysis::LinkedBoxes_FromJsonString(const FString &InJson, TArray<FS_LinkedBox> &OutBoxes)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
	TArray<TSharedPtr<FJsonValue>> Arr;
	if (!FJsonSerializer::Deserialize(Reader, Arr))
	{
		// In case file stored a single object, not an array
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader2 = TJsonReaderFactory<>::Create(InJson);
		if (FJsonSerializer::Deserialize(Reader2, Obj) && Obj.IsValid())
		{
			FS_LinkedBox Single;
			if (JsonObjectToLinkedBox(Obj, Single))
			{
				OutBoxes = {Single};
				return true;
			}
		}
		return false;
	}
	OutBoxes.Reset();
	OutBoxes.Reserve(Arr.Num());
	for (const TSharedPtr<FJsonValue> &V : Arr)
	{
		if (!V.IsValid())
			continue;
		TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid())
			continue;
		FS_LinkedBox B;
		if (JsonObjectToLinkedBox(Obj, B))
		{
			OutBoxes.Add(MoveTemp(B));
		}
	}
	return true;
}

bool UCPP_BPL__VolumeAnalysis::LoadLinkedBoxesFromJsonFile(const FString &FilePath, TArray<FS_LinkedBox> &OutBoxes)
{
	FString Data;
	if (!FFileHelper::LoadFileToString(Data, *FilePath))
	{
		return false;
	}
	return LinkedBoxes_FromJsonString(Data, OutBoxes);
}