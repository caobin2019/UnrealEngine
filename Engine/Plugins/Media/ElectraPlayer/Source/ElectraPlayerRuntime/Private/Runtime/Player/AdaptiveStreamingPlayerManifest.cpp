// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "Misc/Paths.h"
#include "Player/AdaptiveStreamingPlayerInternal.h"
#include "Player/AdaptivePlayerOptionKeynames.h"
#include "Player/HLS/PlaylistReaderHLS.h"
#include "Player/mp4/PlaylistReaderMP4.h"
#include "Player/DASH/PlaylistReaderDASH.h"
#include "Utilities/Utilities.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/URLParser.h"
#include "ParameterDictionary.h"


namespace Electra
{

namespace Playlist
{
static const FString MIMETypeMP4(TEXT("video/mp4"));
static const FString MIMETypeMP4A(TEXT("audio/mp4"));
static const FString MIMETypeHLS(TEXT("application/vnd.apple.mpegURL"));
static const FString MIMETypeDASH(TEXT("application/dash+xml"));


/**
 * Returns the MIME type of the media playlist pointed to by the given URL.
 * This only parses the URL for known extensions or scheme.
 * If the MIME type cannot be precisely determined an empty string is returned.
 *
 * @param URL    Media playlist URL
 *
 * @return MIME type or empty string if it cannot be determined
 */
FString GetMIMETypeForURL(const FString &URL)
{
	FString MimeType;
	FURL_RFC3986 UrlParser;
	if (!UrlParser.Parse(URL))
	{
		return MimeType;
	}

	//	FString PathOnly = UrlParser->GetPath();
	TArray<FString> PathComponents;
	UrlParser.GetPathComponents(PathComponents);
	if (PathComponents.Num())
	{
		FString LowerCaseExtension = FPaths::GetExtension(PathComponents.Last().ToLower());

		// Check for known extensions.
		static const FString kTextMP4(TEXT("mp4"));
		static const FString kTextMP4V(TEXT("m4v"));
		static const FString kTextMP4A(TEXT("m4a"));
		static const FString kTextMPD(TEXT("mpd"));
		static const FString kTextM3U8(TEXT("m3u8"));
		if (LowerCaseExtension == kTextMP4 || LowerCaseExtension == kTextMP4V)
		{
			MimeType = MIMETypeMP4;
		}
		else if (LowerCaseExtension == kTextMP4A)
		{
			MimeType = MIMETypeMP4A;
		}
		else if (LowerCaseExtension == kTextMPD)
		{
			MimeType = MIMETypeDASH;
		}
		else if (LowerCaseExtension == kTextM3U8)
		{
			MimeType = MIMETypeHLS;
		}
	}

	return MimeType;
}


} // namespace Playlist







void FAdaptiveStreamingPlayer::OnManifestGetMimeTypeComplete(TSharedPtrTS<FHTTPResourceRequest> InRequest)
{
	if (!InRequest->GetWasCanceled())
	{
		FString mimeType(TEXT("application/octet-stream"));
		if (InRequest->GetError() == 0)
		{
			const HTTP::FConnectionInfo* ci = InRequest->GetConnectionInfo();
			if (ci)
			{
				mimeType = ci->ContentType;
			}
		}
		WorkerThread.SendLoadManifestMessage(ManifestURL, mimeType);
	}
}


void FAdaptiveStreamingPlayer::InternalCancelLoadManifest()
{
	if (ManifestMimeTypeRequest.IsValid())
	{
		ManifestMimeTypeRequest->Cancel();
		ManifestMimeTypeRequest.Reset();
	}
}


//-----------------------------------------------------------------------------
/**
 * Starts asynchronous loading and parsing of a manifest.
 *
 * @param URL
 * @param MimeType
 */
void FAdaptiveStreamingPlayer::InternalLoadManifest(const FString& URL, const FString& MimeType)
{
	// Remember the original request URL since we may lose the fragment part in requests.
	ManifestURL = URL;
	ManifestMimeTypeRequest.Reset();
	ManifestType = EMediaFormatType::Unknown;
	if (CurrentState == EPlayerState::eState_Idle)
	{
		FString mimeType = MimeType;
		if (GetOptions().HaveKey(OptionKeyMimeType))
		{
			mimeType = GetOptions().GetValue(OptionKeyMimeType).GetFString();
		}
		else if (mimeType.IsEmpty())
		{
			mimeType = Playlist::GetMIMETypeForURL(URL);
		}
		// If there is no mime type we need to issue a HTTP HEAD request in order to get the "Content-Type" header.
		if (mimeType.IsEmpty() && (URL.StartsWith("https://", ESearchCase::CaseSensitive) || URL.StartsWith("http://", ESearchCase::CaseSensitive)))
		{
			ManifestMimeTypeRequest = MakeSharedTS<FHTTPResourceRequest>();
			ManifestMimeTypeRequest->URL(URL);
			ManifestMimeTypeRequest->Verb(TEXT("HEAD"));
			ManifestMimeTypeRequest->Callback().BindThreadSafeSP(AsShared(), &FAdaptiveStreamingPlayer::OnManifestGetMimeTypeComplete);
			ManifestMimeTypeRequest->StartGet(this);
			return;
		}
		if (mimeType.Len())
		{
			check(!ManifestReader.IsValid());

			CurrentState = EPlayerState::eState_ParsingManifest;
			if (mimeType == Playlist::MIMETypeHLS)
			{
				ManifestReader = IPlaylistReaderHLS::Create(this);
				ManifestType = EMediaFormatType::HLS;
			}
			else if (mimeType == Playlist::MIMETypeMP4)
			{
				ManifestReader = IPlaylistReaderMP4::Create(this);
				ManifestType = EMediaFormatType::ISOBMFF;
			}
			else if (mimeType == Playlist::MIMETypeDASH)
			{
				ManifestReader = IPlaylistReaderDASH::Create(this);
				ManifestType = EMediaFormatType::DASH;
			}
			else
			{
				FErrorDetail err;
				err.SetFacility(Facility::EFacility::Player);
				err.SetMessage(TEXT("Unsupported stream MIME type"));
				err.SetCode(INTERR_UNSUPPORTED_FORMAT);
				PostError(err);
			}

			if (ManifestReader.IsValid())
			{
				DispatchEvent(FMetricEvent::ReportOpenSource(URL));
				ManifestReader->LoadAndParse(URL);
			}
		}
		else
		{
			FErrorDetail err;
			err.SetFacility(Facility::EFacility::Player);
			err.SetMessage(TEXT("Could not determine stream MIME type"));
			err.SetCode(INTERR_UNSUPPORTED_FORMAT);
			PostError(err);
		}
	}
	else
	{
// TODO: Error: Not idle
	}
}


//-----------------------------------------------------------------------------
/**
 * Selects the internal presentation for playback after having selected/disabled candidate streams via AccessManifest().
 *
 * @return
 */
bool FAdaptiveStreamingPlayer::SelectManifest()
{
	if (ManifestReader)
	{
		check(Manifest == nullptr);
		if (ManifestType != EMediaFormatType::Unknown)
		{
			TArray<FTimespan> SeekablePositions;
			TSharedPtrTS<IManifest> NewPresentation = ManifestReader->GetManifest();
			check(NewPresentation.IsValid());

			PlaybackState.SetSeekableRange(NewPresentation->GetSeekableTimeRange());
			NewPresentation->GetSeekablePositions(SeekablePositions);
			PlaybackState.SetSeekablePositions(SeekablePositions);
			PlaybackState.SetTimelineRange(NewPresentation->GetTotalTimeRange());
			PlaybackState.SetDuration(NewPresentation->GetDuration());

			TArray<FTrackMetadata> VideoTrackMetadata;
			TArray<FTrackMetadata> AudioTrackMetadata;
			NewPresentation->GetTrackMetadata(VideoTrackMetadata, EStreamType::Video);
			NewPresentation->GetTrackMetadata(AudioTrackMetadata, EStreamType::Audio);
			PlaybackState.SetTrackMetadata(VideoTrackMetadata, AudioTrackMetadata);
			PlaybackState.SetHaveMetadata(true);

			Manifest = NewPresentation;

			CurrentState = EPlayerState::eState_Ready;

			double minBufTimeMPD = Manifest->GetMinBufferTime().GetAsSeconds();
			PlayerConfig.InitialBufferMinTimeAvailBeforePlayback = Utils::Min(minBufTimeMPD, PlayerConfig.InitialBufferMinTimeAvailBeforePlayback);
			PlayerConfig.SeekBufferMinTimeAvailBeforePlayback    = Utils::Min(minBufTimeMPD, PlayerConfig.SeekBufferMinTimeAvailBeforePlayback);
			PlayerConfig.RebufferMinTimeAvailBeforePlayback 	 = Utils::Min(minBufTimeMPD, PlayerConfig.RebufferMinTimeAvailBeforePlayback);

			return true;
		}
		else
		{
			// Handle other types of playlist here.
			FErrorDetail err;
			err.SetFacility(Facility::EFacility::Player);
			err.SetMessage(TEXT("Unsupported playlist/manifest type"));
			err.SetCode(INTERR_UNSUPPORTED_FORMAT);
			PostError(err);
		}
	}

	return false;
}

void FAdaptiveStreamingPlayer::UpdateManifest()
{
	if (Manifest.IsValid())
	{
		TArray<FTimespan> SeekablePositions;
		PlaybackState.SetSeekableRange(Manifest->GetSeekableTimeRange());
		Manifest->GetSeekablePositions(SeekablePositions);
		PlaybackState.SetSeekablePositions(SeekablePositions);
		PlaybackState.SetTimelineRange(Manifest->GetTotalTimeRange());
		PlaybackState.SetDuration(Manifest->GetDuration());
	}
}

} // namespace Electra


