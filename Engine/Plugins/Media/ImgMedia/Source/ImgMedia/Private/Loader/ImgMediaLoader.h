// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/LruCache.h"
#include "Containers/Queue.h"
#include "IMediaSamples.h"
#include "Misc/FrameRate.h"
#include "Templates/SharedPointer.h"

class FImgMediaGlobalCache;
class FImgMediaLoaderWork;
class FImgMediaMipMapInfo;
class FImgMediaScheduler;
class FImgMediaTextureSample;
class FMediaTimeStamp;
class IImageWrapperModule;
class IImgMediaReader;
class IQueuedWork;

struct FImgMediaFrame;
struct FImgMediaTileSelection;


/**
 * Loads image sequence frames from disk.
 */
class FImgMediaLoader
	: public TSharedFromThis<FImgMediaLoader, ESPMode::ThreadSafe>
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InScheduler The scheduler for image loading.
	 */
	FImgMediaLoader(const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
		const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache,
		const TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe>& InMipMapInfo);

	/** Virtual destructor. */
	virtual ~FImgMediaLoader();

public:
	/** Max number of mip map levels supported. */
	static constexpr int32 MAX_MIPMAP_LEVELS = 32;

	/**
	 * Get the data bit rate of the video frames.
	 *
	 * @return Data rate (in bits per second).
	 */
	uint64 GetBitRate() const;

	/**
	 * Get the time ranges of frames that are being loaded right now.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetCompletedTimeRanges, GetPendingTimeRanges
	 */
	void GetBusyTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the time ranges of frames that are are already loaded.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetBusyTimeRanges, GetPendingTimeRanges
	 */
	void GetCompletedTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the image frame at the specified time.
	 *
	 * @param Time The time of the image frame to get (relative to the beginning of the sequence).
	 * @return The frame, or nullptr if the frame wasn't available yet.
	 */
	TSharedPtr<FImgMediaTextureSample, ESPMode::ThreadSafe> GetFrameSample(FTimespan Time);

	/**
	 * Get the information string for the currently loaded image sequence.
	 *
	 * @return Info string.
	 * @see GetSequenceDuration, GetSamples
	 */
	const FString& GetInfo() const
	{
		return Info;
	}

	/**
	 * Tries to get the best sample for a given time range.
	 *
	 * @param TimeRange is the range to check for.
	 * @param OutSample will be filled in with the sample if found.
	 * @param bIsLoopingEnabled True if we can loop.
	 * @param PlayRate How fast we are playing.
	 * @param Facade does use blocking playback to fetch samples
	 * @return True if successful.
	 */
	IMediaSamples::EFetchBestSampleResult FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bIsLoopingEnabled, float PlayRate, bool bPlaybackIsBlocking);

	/**
	 * Checks to see if a sample is available at the specificed time.
	 *
	 * @param TimeStamp Will be filled in with the time stamp of the sample if a sample is found.
	 * @param bIsLoopingEnabled True if we can loop.
	 * @param PlayRate How fast we are playing.
	 * @param CurrentTime Time to get a sample for.
	 * @return True if a sample is available.
	 */
	bool PeekVideoSampleTime(FMediaTimeStamp& TimeStamp, bool bIsLoopingEnabled, float PlayRate, const FTimespan& CurrentTime);

	/**
	 * Get the time ranges of frames that are pending to be loaded.
	 *
	 * @param OutRangeSet Will contain the set of time ranges.
	 * @return true on success, false otherwise.
	 * @see GetBusyTimeRanges, GetCompletedTimeRanges
	 */
	void GetPendingTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the image reader object used by this loader.
	 *
	 * @return The image reader.
	 */
	TSharedRef<IImgMediaReader, ESPMode::ThreadSafe> GetReader() const
	{
		return Reader.ToSharedRef();
	}

	/**
	 * Get the width and height of the image sequence.
	 *
	 * The dimensions of the image sequence are determined by reading the attributes
	 * of the first image. The dimensions of individual image frames in the sequence
	 * are allowed to differ. However, this usually indicates a mistake in the content
	 * creation pipeline and will be logged out as such.
	 *
	 * @return Sequence dimensions.
	 * @see GetSequenceDuration, GetSequenceFps
	 */
	const FIntPoint& GetSequenceDim() const
	{
		return SequenceDim;
	}

	/**
	 * Get the total duration of the image sequence.
	 *
	 * @return Duration.
	 * @see GetSequenceDim, GetSequenceFps
	 */
	FTimespan GetSequenceDuration() const
	{
		return SequenceDuration;
	}

	/**
	 * Get the sequence's frame rate.
	 *
	 * The frame rate of the image sequence is determined by reading the attributes
	 * of the first image. Individual image frames may specify a different frame rate,
	 * but it will be ignored during playback.
	 *
	 * @return The frame rate.
	 * @see GetSequenceDuration, GetSequenceDim
	 */
	FFrameRate GetSequenceFrameRate() const
	{
		return SequenceFrameRate;
	}

	/**
	 * Get the path to images in the sequence.
	 *
	 * @param FrameNumber Frame to get.
	 * @param MipLevel Mip level to get.
	 * @return Returns path and filename of image.
	 */
	const FString& GetImagePath(int32 FrameNumber, int32 MipLevel) const;

	/**
	 * Get the number of mipmap levels we have.
	 */
	int32 GetNumMipLevels() const
	{
		return ImagePaths.Num();
	}

	/**
	 * Get the number of images in a single mip level.
	 *
	 * @return Number of images.
	 */
	int32 GetNumImages() const
	{
		return ImagePaths.Num() > 0 ? ImagePaths[0].Num() : 0;
	}

	/**
	 * Get the next work item.
	 *
	 * This method is called by the scheduler.
	 *
	 * @return The work item, or nullptr if no work is available.
	 */
	IQueuedWork* GetWork();

	/**
	 * Initialize the image sequence loader.
	 *
	 * @param SequencePath Path to the image sequence.
	 * @param FrameRateOverride The frame rate to use (0/0 = do not override).
	 * @param Loop Whether the cache should loop around.
	 * @see IsInitialized
	 */
	void Initialize(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop);

	/**
	 * Whether this loader has been initialized yet.
	 *
	 * @return true if initialized, false otherwise.
	 * @see Initialize
	 */
	bool IsInitialized() const
	{
		return Initialized;
	}

	/**
	 * Notify the loader that a work item completed.
	 *
	 * @param CompletedWork The work item that completed.
	 * @param FrameNumber Number of the frame that was read.
	 * @param Frame The frame that was read, or nullptr if reading failed.
	 */
	void NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame);

	/**
	 * Asynchronously request the image frame at the specified time.
	 *
	 * @param Time The time of the image frame to request (relative to the beginning of the sequence).
	 * @param PlayRate The current play rate (used by the look-ahead logic).
	 * @param Loop Whether the cache should loop around.
	 * @return bool if the frame will be loaded, false otherwise.
	 */
	bool RequestFrame(FTimespan Time, float PlayRate, bool Loop);

	/**
	 * Reset "queued fetch" related state used to emulate player output queue behavior
	 */
	void ResetFetchLogic();

protected:

	/**
	 * Convert a collection of frame numbers to corresponding time ranges.
	 *
	 * @param FrameNumbers The frame numbers to convert.
	 * @param OutRangeSet Will contain the time ranges.
	 */
	void FrameNumbersToTimeRanges(const TArray<int32>& FrameNumbers, TRangeSet<FTimespan>& OutRangeSet) const;

	/**
	 * Get the play head time corresponding to the specified frame number.
	 *
	 * @param FrameNumber The frame number.
	 * @return The corresponding time.
	 * @see TimeToFrameNumber
	 */
	FTimespan FrameNumberToTime(uint32 FrameNumber) const;

	/**
	 * Initialize the image sequence loader.
	 *
	 * @param SequencePath Path to the image sequence.
	 * @param FrameRateOverride The frame rate to use (0/0 = do not override).
	 * @param Loop Whether the cache should loop around.
	 */
	void LoadSequence(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop);

	/**
	 * Finds all the files in a directory and gets their path.
	 *
	 * @param SequencePath Directory to look in.
	 * @param OutputPaths File paths will be added to this.
	 */
	void FindFiles(const FString& SequencePath, TArray<FString>& OutputPaths);

	/**
	 * Finds the mip map files for this sequence (if any).
	 *
	 * Typically with non mips, a single directory holds all the files of a single sequence.
	 *
	 * With mip maps, a directory will hold all the files of a single sequence of a specific mip level.
	 * The naming convention is for the directory name to end in _<SIZE>.
	 *   SIZE does not need to be a power of 2.
	 *   Each subsequent level should have SIZE be half of the level preceeding it.
	 *   If SIZE does not divide evenly by 2, then round down.
	 * The part of the name preceding _<SIZE> should be the same for all mip levels.
	 * All mip levels of the same sequence should be in the same location
	 * E.g. /Sequence/Seq_256/, /Sequence/Seq_128/, /Sequence/Seq_64/, etc.

	 * FindMips will look for mip levels that are the at the level of SequencePath and below.
	 * E.g. if SequencePath is Seq_1024, then FindMips will look for Seq_1024, Seq_512, etc
	 * and will NOT look for Seq_2048 even if it is present.
	 *
	 * @param SequencePath Directory of sequence.
	 */
	void FindMips(const FString& SequencePath);

	/**
	 * Get the frame number corresponding to the specified play head time.
	 *
	 * @param Time The play head time.
	 * @return The corresponding frame number, or INDEX_NONE.
	 * @see FrameNumberToTime
	 */
	uint32 TimeToFrameNumber(FTimespan Time) const;

	/**
	 * Update the loader based on the current play position.
	 *
	 * @param PlayHeadFrame Current play head frame number.
	 * @param PlayRate Current play rate.
	 * @param Loop Whether the cache should loop around.
	 */
	void Update(int32 PlayHeadFrame, float PlayRate, bool Loop);

	/**
	 * Get what mip level we should be using for a given frame.
	 *
	 * @param FrameIndex Frame to get mip level for.
	 * @param OutTileSelection Will be filled in with what tiles are actually needed.
	 * @return Returns the mip level.
	 */
	int32 GetDesiredMipLevel(int32 FrameIndex, FImgMediaTileSelection& OutTileSelection);

	/***
	 * Modulos the time so that it is between 0 and SequenceDuration.
	 * Handles negative numbers appropriately.
	 */
	FTimespan ModuloTime(FTimespan Time);

	/**
	 * Gets the amount of overlap (in seconds) between a frame and a time range.
	 * A negative or zero  value indicates the frame does not overlap the range.
	 *
	 * @param FrameIndex Index of frame to check.
	 * @param StartTime Start of the range.
	 * @param EndTime End of the range.
	 * @param Amount of overlap.
	 */
	float GetFrameOverlap(uint32 FrameIndex, FTimespan StartTime, FTimespan EndTime) const;

	/**
	 * Find maximum overlapping frame index for given range
	 */
	float FindMaxOverlapInRange(int32 StartIndex, int32 EndIndex, FTimespan StartTime, FTimespan EndTime, int32& MaxIdx) const;

	/**
	 * Get frame data for given index. If not available attempt to find earlier frame up to given index
	 */
	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* GetFrameForBestIndex(int32 & MaxIdx, int32 LastIndex);

private:

	/** Critical section for synchronizing access to Frames. */
	mutable FCriticalSection CriticalSection;

	/** The currently loaded image sequence frames. */
	TLruCache<int32, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>> Frames;

	/** The image wrapper module to use. */
	IImageWrapperModule& ImageWrapperModule;

	/**
	 * Paths to each image for each mip map level in the currently opened sequence.
	 * This is an array of mip levels, and each mip level is an array of image paths.
	 */
	TArray<TArray<FString>> ImagePaths;

	/** Media information string. */
	FString Info;

	/** Whether this loader has been initialized yet. */
	bool Initialized;

	/** The number of frames to load ahead of the play head. */
	int32 NumLoadAhead;

	/** The number of frames to load behind the play head. */
	int32 NumLoadBehind;

	/** The image sequence reader to use. */
	TSharedPtr<IImgMediaReader, ESPMode::ThreadSafe> Reader;

	/** The scheduler for image loading. */
	TSharedPtr<FImgMediaScheduler, ESPMode::ThreadSafe> Scheduler;

	/** The scheduler for image loading. */
	TSharedPtr<FImgMediaGlobalCache, ESPMode::ThreadSafe> GlobalCache;

	/** MipMapInfo object used to handle mipmaps. Could be null if we have no mipmaps. */
	TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe> MipMapInfo;

	/** Width and height of the image sequence (in pixels) .*/
	FIntPoint SequenceDim;

	/** Total length of the image sequence. */
	FTimespan SequenceDuration;

	/** Frame rate of the currently loaded sequence. */
	FFrameRate SequenceFrameRate;

	/** Identifying name of sequence files. */
	FName SequenceName;

private:

	/** Index of the previously requested frame. */
	int32 LastRequestedFrame;

	/** Collection of frame numbers that need to be read. */
	TArray<int32> PendingFrameNumbers;

	/** Collection of frame numbers that are being read. */
	TArray<int32> QueuedFrameNumbers;

	/** Object pool for reusable work items. */
	TArray<FImgMediaLoaderWork*> WorkPool;

	/** True if we are using the global cache, false to use the local cache. */
	bool UseGlobalCache;

	/** State related to "queue style" frame access functions */
	struct
	{
		int32 LastFrameIndex;
		uint64 CurrentSequenceIndex;
	} QueuedSampleFetch;

};
