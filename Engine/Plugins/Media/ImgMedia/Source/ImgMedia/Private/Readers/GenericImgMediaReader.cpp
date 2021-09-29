// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericImgMediaReader.h"
#include "ImgMediaPrivate.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImgMediaLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


/* Local helpers
 *****************************************************************************/

TSharedPtr<IImageWrapper> LoadImage(const FString& ImagePath, IImageWrapperModule& ImageWrapperModule, TArray64<uint8>& OutBuffer, FImgMediaFrameInfo& OutInfo)
{
	// load image into buffer
	if (!FFileHelper::LoadFileToArray(OutBuffer, *ImagePath))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load %s"), *ImagePath);
		return nullptr;
	}

	// determine image format
	EImageFormat ImageFormat;

	const FString Extension = FPaths::GetExtension(ImagePath).ToLower();

	if (Extension == TEXT("bmp"))
	{
		ImageFormat = EImageFormat::BMP;
		OutInfo.FormatName = TEXT("BMP");
	}
	else if ((Extension == TEXT("jpg")) || (Extension == TEXT("jpeg")))
	{
		ImageFormat = EImageFormat::JPEG;
		OutInfo.FormatName = TEXT("JPEG");
	}
	else if (Extension == TEXT("png"))
	{
		ImageFormat = EImageFormat::PNG;
		OutInfo.FormatName = TEXT("PNG");
	}
	else
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Unsupported file format in %s"), *ImagePath);
		return nullptr;
	}

	// create image wrapper
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(OutBuffer.GetData(), OutBuffer.Num()))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to create image wrapper for %s"), *ImagePath);
		return nullptr;
	}

	// get file info
	const UImgMediaSettings* Settings = GetDefault<UImgMediaSettings>();
	OutInfo.CompressionName = TEXT("");
	OutInfo.Dim.X = ImageWrapper->GetWidth();
	OutInfo.Dim.Y = ImageWrapper->GetHeight();
	OutInfo.FrameRate = Settings->DefaultFrameRate;
	OutInfo.Srgb = true;
	OutInfo.UncompressedSize = (SIZE_T)OutInfo.Dim.X * OutInfo.Dim.Y * 4;

	return ImageWrapper;
}


/* FGenericImgMediaReader structors
 *****************************************************************************/

FGenericImgMediaReader::FGenericImgMediaReader(IImageWrapperModule& InImageWrapperModule, const TSharedRef<FImgMediaLoader, ESPMode::ThreadSafe>& InLoader)
	: ImageWrapperModule(InImageWrapperModule)
	, LoaderPtr(InLoader)
{ }


/* FGenericImgMediaReader interface
 *****************************************************************************/

bool FGenericImgMediaReader::GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo)
{
	TArray64<uint8> InputBuffer;
	TSharedPtr<IImageWrapper> ImageWrapper = LoadImage(ImagePath, ImageWrapperModule, InputBuffer, OutInfo);

	return ImageWrapper.IsValid();
}


bool FGenericImgMediaReader::ReadFrame(int32 FrameId, int32 MipLevel, const FImgMediaTileSelection& InTileSelectio, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> OutFrame)
{
	TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> Loader = LoaderPtr.Pin();
	if (Loader.IsValid() == false)
	{
		return false;
	}

	int32 NumMipLevels = Loader->GetNumMipLevels();
	FIntPoint Dim = Loader->GetSequenceDim();
	
	// Do we already have our buffer?
	void* Buffer = OutFrame->Data.Get();

	// Loop over all mips.
	for (int32 CurrentMipLevel = 0; CurrentMipLevel < NumMipLevels; ++CurrentMipLevel)
	{
		// Do we want to read in this mip?
		bool IsThisLevelPresent = (OutFrame->MipMapsPresent & (1 << CurrentMipLevel)) != 0;
		bool ReadThisMip = (Buffer == nullptr) ||
			((CurrentMipLevel >= MipLevel) && (IsThisLevelPresent == false));
		if (ReadThisMip)
		{
			// Load image.
			const FString& ImagePath = Loader->GetImagePath(FrameId, CurrentMipLevel);

			TArray64<uint8> InputBuffer;
			FImgMediaFrameInfo Info;
			TSharedPtr<IImageWrapper> ImageWrapper = LoadImage(ImagePath, ImageWrapperModule, InputBuffer, Info);

			if (!ImageWrapper.IsValid())
			{
				UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load image %s"), *ImagePath);
				return false;
			}

			TArray64<uint8> RawData;
			if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			{
				UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to get image data for %s"), *ImagePath);
				return false;
			}

			const int64 RawNum = RawData.Num();
			// Create buffer for data.
			if (Buffer == nullptr)
			{
				int64 AllocSize = RawNum;
				// Need more space for mips.
				if (NumMipLevels > 1)
				{
					AllocSize = (AllocSize * 4) / 3;
				}
				Buffer = FMemory::Malloc(AllocSize);
				OutFrame->Info = Info;
				OutFrame->Data = MakeShareable(Buffer, [](void* ObjectToDelete) { FMemory::Free(ObjectToDelete); });
				OutFrame->Format = EMediaTextureSampleFormat::CharBGRA;
				OutFrame->Stride = OutFrame->Info.Dim.X * 4;
			}

			// Copy data to our buffer
			FMemory::Memcpy(Buffer, RawData.GetData(), RawNum);
			OutFrame->MipMapsPresent |= 1 << CurrentMipLevel;
		}

		// Next level.
		Buffer = (void*)((uint8*)Buffer + Dim.X * Dim.Y * 4);
		Dim /= 2;
	}

	return true;
}
