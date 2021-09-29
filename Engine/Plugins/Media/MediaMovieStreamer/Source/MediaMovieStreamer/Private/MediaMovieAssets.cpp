// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaMovieAssets.h"
#include "MediaMovieStreamer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"

UMediaMovieAssets::UMediaMovieAssets()
	: MediaPlayer(nullptr)
	, MediaSource(nullptr)
	, MediaTexture(nullptr)
	, MovieStreamer(nullptr)
{
}

UMediaMovieAssets::~UMediaMovieAssets()
{
}

void UMediaMovieAssets::SetMediaPlayer(UMediaPlayer* InMediaPlayer, FMediaMovieStreamer* InMovieStreamer)
{
	// Unbind from previous player.
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnEndReached.RemoveDynamic(this, &UMediaMovieAssets::OnMediaEnd);
	}

	MediaPlayer = InMediaPlayer;
	MovieStreamer = InMovieStreamer;
	
	// Bind to new player.
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnEndReached.AddUniqueDynamic(this, &UMediaMovieAssets::OnMediaEnd);
	}
}

void UMediaMovieAssets::SetMediaSource(UMediaSource* InMediaSource)
{
	MediaSource = InMediaSource;
}

void UMediaMovieAssets::SetMediaTexture(UMediaTexture* InMediaTexture)
{
	MediaTexture = InMediaTexture;
}

void UMediaMovieAssets::OnMediaEnd()
{
	if (MovieStreamer != nullptr)
	{
		MovieStreamer->OnMediaEnd();
	}
}

