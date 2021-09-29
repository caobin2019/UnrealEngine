// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#include "BinkMediaTexture.h"
#include "BinkMediaPlayerPCH.h"
#include "BinkMediaPlayer.h"
#include "BinkMediaTextureResource.h"

UBinkMediaTexture::UBinkMediaTexture( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, MediaPlayer(nullptr)
	, Tonemap(false)
	, OutputNits(10000)
	, Alpha(1)
	, CachedDimensions()
	, CurrentMediaPlayer()
	, ReleasePlayerFence()
{
	NeverStream = true;
	if (bink_gpu_api_hdr) 
	{
		PixelFormat = PF_A2B10G10R10;
		DecodeSRGB = true;
	} 
	else 
	{
		PixelFormat = PF_B8G8R8A8;
		DecodeSRGB = false;
	}

	UpdateResource();
}

UBinkMediaTexture::~UBinkMediaTexture() 
{
}

void UBinkMediaTexture::SetMediaPlayer( UBinkMediaPlayer* InMediaPlayer ) 
{
	MediaPlayer = InMediaPlayer;
	InitializeTrack();
}

void UBinkMediaTexture::Clear() 
{
	if (Resource) 
	{
		FBinkMediaTextureResource *res = (FBinkMediaTextureResource *)Resource;
		res->Clear();
	}
}

void UBinkMediaTexture::BeginDestroy() 
{
	Super::BeginDestroy();

	// synchronize with the rendering thread by inserting a fence
 	if (!ReleasePlayerFence) {
 		ReleasePlayerFence = new FRenderCommandFence();
 	}

 	ReleasePlayerFence->BeginFence();
}

void UBinkMediaTexture::FinishDestroy() 
{
	delete ReleasePlayerFence;
	ReleasePlayerFence = nullptr;

	Super::FinishDestroy();
}

FString UBinkMediaTexture::GetDesc() 
{
	return FString::Printf(TEXT("%dx%d [%s]"), CachedDimensions.X, CachedDimensions.Y, GPixelFormats[PixelFormat].Name);
}

void UBinkMediaTexture::PostLoad() 
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine) 
	{
		InitializeTrack();
	}
}

#if BINKPLUGIN_UE4_EDITOR

void UBinkMediaTexture::PreEditChange( FProperty* PropertyAboutToChange ) 
{
	// this will release the FMediaTextureResource
	Super::PreEditChange(PropertyAboutToChange);

	FlushRenderingCommands();
}


void UBinkMediaTexture::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) 
{
	InitializeTrack();

	// this will recreate the FMediaTextureResource
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // BINKPLUGIN_UE4_EDITOR

void UBinkMediaTexture::InitializeTrack() 
{
	// assign new media player asset
	if (CurrentMediaPlayer != MediaPlayer) 
	{
		if (CurrentMediaPlayer) 
		{
			CurrentMediaPlayer->OnMediaChanged().RemoveAll(this);
		}

		CurrentMediaPlayer = MediaPlayer;

		if (MediaPlayer) 
		{
			MediaPlayer->OnMediaChanged().AddUObject(this, &UBinkMediaTexture::HandleMediaPlayerMediaChanged);
		}	
	}

	if (MediaPlayer) 
	{
		CachedDimensions = MediaPlayer->GetDimensions();
	}
	else 
	{
		CachedDimensions = FIntPoint(ForceInit);
	}

	UpdateResource();
}

void UBinkMediaTexture::HandleMediaPlayerMediaChanged() 
{
	InitializeTrack();
}

FTextureResource* UBinkMediaTexture::CreateResource() 
{ 
	if (CachedDimensions.X == 0 || CachedDimensions.Y == 0)
	{
		return nullptr;
	}
	return new FBinkMediaTextureResource(this, PixelFormat); 
}
