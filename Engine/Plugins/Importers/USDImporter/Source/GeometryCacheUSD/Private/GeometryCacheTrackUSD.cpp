// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheTrackUSD.h"

#include "GeometryCacheUSDStream.h"
#include "IGeometryCacheStreamer.h"

UGeometryCacheTrackUsd::UGeometryCacheTrackUsd()
: StartFrameIndex(0)
, EndFrameIndex(0)
{
}

UGeometryCacheTrackUsd::~UGeometryCacheTrackUsd()
{
	IGeometryCacheStreamer::Get().UnregisterTrack(this);
}

const bool UGeometryCacheTrackUsd::UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData)
{
	const int32 SampleIndex = FindSampleIndexFromTime(Time, bLooping);

	// If InOutMeshSampleIndex equals -1 (first creation) update the OutVertices and InOutMeshSampleIndex
	// Update the Vertices and Index if SampleIndex is different from the stored InOutMeshSampleIndex
	if (InOutMeshSampleIndex == -1 || SampleIndex != InOutMeshSampleIndex)
	{
		if (GetMeshData(SampleIndex, MeshData))
		{
			OutMeshData = &MeshData;
			InOutMeshSampleIndex = SampleIndex;
			return true;
		}
	}
	return false;
}

const bool UGeometryCacheTrackUsd::UpdateBoundsData(const float Time, const bool bLooping, const bool bIsPlayingBackward, int32& InOutBoundsSampleIndex, FBox& OutBounds)
{
	const int32 SampleIndex = FindSampleIndexFromTime(Time, bLooping);

	const FGeometryCacheTrackSampleInfo& SampledInfo = GetSampleInfo(Time, bLooping);
	if (InOutBoundsSampleIndex != SampleIndex)
	{
		OutBounds = SampledInfo.BoundingBox;
		InOutBoundsSampleIndex = SampleIndex;
		return true;
	}
	return false;
}

const int32 UGeometryCacheTrackUsd::FindSampleIndexFromTime(const float Time, const bool bLooping) const
{
	// Treat the time as the frame index
	int32 FrameIndex = (int32) Time;
	return FMath::Clamp(FrameIndex, StartFrameIndex, EndFrameIndex - 1);
}

const FGeometryCacheTrackSampleInfo& UGeometryCacheTrackUsd::GetSampleInfo(float Time, bool bLooping)
{
	if (SampleInfos.Num() == 0)
	{
		if (Duration > 0.f)
		{
			// Duration is the number of frames
			SampleInfos.SetNum((int32)Duration);
		}
		else
		{
			return FGeometryCacheTrackSampleInfo::EmptySampleInfo;
		}
	}

	// The sample info index must start from 0, while the sample index is between the range of the animation
	const int32 SampleInfoIndex = FindSampleIndexFromTime(Time, bLooping) - StartFrameIndex;

	FGeometryCacheTrackSampleInfo& CurrentSampleInfo = SampleInfos[SampleInfoIndex];

	if (CurrentSampleInfo.SampleTime == 0.0f && CurrentSampleInfo.NumVertices == 0 && CurrentSampleInfo.NumIndices == 0)
	{
		FGeometryCacheMeshData TempMeshData;
		GetMeshData(SampleInfoIndex, TempMeshData);

		CurrentSampleInfo = FGeometryCacheTrackSampleInfo(
			Time,
			MeshData.BoundingBox,
			MeshData.Positions.Num(),
			MeshData.Indices.Num()
		);
	}

	return CurrentSampleInfo;
}

bool UGeometryCacheTrackUsd::GetMeshData(int32 SampleIndex, FGeometryCacheMeshData& OutMeshData)
{
	if (IGeometryCacheStreamer::Get().IsTrackRegistered(this))
	{
		return IGeometryCacheStreamer::Get().TryGetFrameData(this, SampleIndex, OutMeshData);
	}
	return false;
}

void UGeometryCacheTrackUsd::Initialize(FReadUsdMeshFunction InReadFunc, const FString& InPrimpath, int32 InStartFrameIndex, int32 InEndFrameIndex)
{
	StartFrameIndex = InStartFrameIndex;
	EndFrameIndex = InEndFrameIndex;
	Duration = (float) (EndFrameIndex - StartFrameIndex);

	// Setup the corresponding stream
	FGeometryCacheUsdStream* Stream = new FGeometryCacheUsdStream(this, InReadFunc, InPrimpath);

	IGeometryCacheStreamer& Streamer = IGeometryCacheStreamer::Get();
	Streamer.RegisterTrack(this, Stream);

	Stream->Prefetch(StartFrameIndex);
	GetMeshData(StartFrameIndex, MeshData);
}
