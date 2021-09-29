// Copyright Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "RemoteControlField.h"
#include "RemoteControlModels.h"
#include "RemoteControlPreset.h"
#include "RemoteControlRoute.h"


#include "RemoteControlResponse.generated.h"

USTRUCT()
struct FAPIInfoResponse
{
	GENERATED_BODY()
	
	FAPIInfoResponse() = default;

	FAPIInfoResponse(const TArray<FRemoteControlRoute>& InRoutes, bool bInPackaged, URemoteControlPreset* InActivePreset)
		: IsPackaged(bInPackaged)
		, ActivePreset(InActivePreset) 
	{
		HttpRoutes.Append(InRoutes);
	}

private:
	/**
	 * Whether this is a packaged build or not.
	 */
	bool IsPackaged = false;

	/**
	 * Descriptions for all the routes that make up the remote control API.
	 */
	UPROPERTY()
	TArray<FRemoteControlRouteDescription> HttpRoutes;

	UPROPERTY()
	FRCShortPresetDescription ActivePreset;
};

USTRUCT()
struct FListPresetsResponse
{
	GENERATED_BODY()
	
	FListPresetsResponse() = default;

	FListPresetsResponse(const TArray<FAssetData>& InPresets)
	{
		Presets.Append(InPresets);
	}

	/**
	 * The list of available remote control presets. 
	 */
	UPROPERTY()
	TArray<FRCShortPresetDescription> Presets;
};

USTRUCT()
struct FGetPresetResponse
{
	GENERATED_BODY()

	FGetPresetResponse() = default;

	FGetPresetResponse(const URemoteControlPreset* InPreset)
		: Preset(InPreset)
	{}

	UPROPERTY()
	FRCPresetDescription Preset;
};

USTRUCT()
struct FDescribeObjectResponse
{
	GENERATED_BODY()

	FDescribeObjectResponse() = default;

	FDescribeObjectResponse(UObject* Object)
		: Name(Object->GetName())
		, Class(Object->GetClass())
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected | CPF_NativeAccessSpecifierPrivate | CPF_DisableEditOnInstance))
			{
				Properties.Emplace(*It);
			}
		}

		for (TFieldIterator<UFunction> It(Object->GetClass()); It; ++It)
		{
			if (It->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Public))
			{
				Functions.Emplace(*It);
			}
		}
	}

	UPROPERTY()
	FString Name;

	UPROPERTY()
	UClass* Class = nullptr;

	UPROPERTY()
	TArray<FRCPropertyDescription> Properties;

	UPROPERTY()
	TArray<FRCFunctionDescription> Functions;
};


USTRUCT()
struct FSearchAssetResponse
{
	GENERATED_BODY()

	FSearchAssetResponse() = default;

	FSearchAssetResponse(const TArray<FAssetData>& InAssets)
	{
		Assets.Append(InAssets);
	}

	UPROPERTY()
	TArray<FRCAssetDescription> Assets;
};


USTRUCT()
struct FSearchActorResponse
{
	GENERATED_BODY()

	FSearchActorResponse() = default;

	FSearchActorResponse(const TArray<AActor*>& InActors)
	{
		Actors.Append(InActors);
	}

	UPROPERTY()
	TArray<FRCObjectDescription> Actors;
};

USTRUCT()
struct FGetMetadataFieldResponse
{
	GENERATED_BODY()

	FGetMetadataFieldResponse() = default;

	FGetMetadataFieldResponse(FString InValue)
		: Value(MoveTemp(InValue))
	{}

	/** The metadata value for a given key. */
	UPROPERTY()
	FString Value;
};


USTRUCT()
struct FGetMetadataResponse
{
	GENERATED_BODY()

	FGetMetadataResponse() = default;

	FGetMetadataResponse(TMap<FString, FString> InMetadata)
		: Metadata(MoveTemp(InMetadata))
	{}

	UPROPERTY()
	TMap<FString, FString> Metadata;
};

USTRUCT()
struct FSetEntityLabelResponse
{
	GENERATED_BODY()

	FSetEntityLabelResponse() = default;

	FSetEntityLabelResponse(FString&& InString)
		: AssignedLabel(MoveTemp(InString))
	{
	}

	/** The label that was assigned when requesting to modify an entity's label. */
	UPROPERTY()
	FString AssignedLabel;
};

USTRUCT()
struct FRCPresetFieldsRenamedEvent
{
	GENERATED_BODY()

	FRCPresetFieldsRenamedEvent() = default;

	FRCPresetFieldsRenamedEvent(FName InPresetName, FGuid InPresetId, TArray<TTuple<FName, FName>> InRenamedFields)
		: Type(TEXT("PresetFieldsRenamed"))
		, PresetName(InPresetName)
		, PresetId(InPresetId.ToString())
		, RenamedFields(MoveTemp(InRenamedFields))
	{
	}

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FName PresetName;

	UPROPERTY()
	FString PresetId;

	UPROPERTY()
	TArray<FRCPresetFieldRenamed> RenamedFields;
};

USTRUCT()
struct FRCPresetMetadataModified
{
	GENERATED_BODY()

	FRCPresetMetadataModified() = default;

	FRCPresetMetadataModified(URemoteControlPreset* InPreset)
		: Type(TEXT("PresetMetadataModified"))
	{
		if (InPreset)
		{
			PresetName = InPreset->GetFName();
			PresetId = InPreset->GetPresetId().ToString();
			Metadata = InPreset->Metadata;
		}
	}

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FName PresetName;

	UPROPERTY()
	FString PresetId;

	UPROPERTY()
	TMap<FString, FString> Metadata;
};


USTRUCT()
struct FRCPresetLayoutModified
{
	GENERATED_BODY()

	FRCPresetLayoutModified() = default;

	FRCPresetLayoutModified(URemoteControlPreset* InPreset)
		: Type(TEXT("PresetLayoutModified"))
		, Preset(InPreset)
	{
	}

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FRCPresetDescription Preset;
};

USTRUCT()
struct FRCPresetFieldsRemovedEvent
{
	GENERATED_BODY()

	FRCPresetFieldsRemovedEvent() = default;

	FRCPresetFieldsRemovedEvent(FName InPresetName, FGuid InPresetId, TArray<FName> InRemovedFields, const TArray<FGuid>& InRemovedFieldIDs)
		: Type(TEXT("PresetFieldsRemoved"))
		, PresetName(InPresetName)
		, PresetId(InPresetId.ToString())
		, RemovedFields(MoveTemp(InRemovedFields))
	{
		Algo::Transform(InRemovedFieldIDs, RemovedFieldIds, [](const FGuid& Id){ return Id.ToString(); });
	}

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FName PresetName;

	UPROPERTY()
	FString PresetId;

	UPROPERTY()
	TArray<FName> RemovedFields;

	UPROPERTY()
	TArray<FString> RemovedFieldIds;
};

USTRUCT()
struct FRCPresetFieldsAddedEvent
{
	GENERATED_BODY()

	FRCPresetFieldsAddedEvent() = default;

	FRCPresetFieldsAddedEvent(FName InPresetName, FGuid InPresetId, FRCPresetDescription InPresetAddition)
		: Type(TEXT("PresetFieldsAdded"))
		, PresetName(InPresetName)
		, PresetId(InPresetId.ToString())
		, Description(MoveTemp(InPresetAddition))
	{
	}

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FName PresetName;

	UPROPERTY()
	FString PresetId;

	UPROPERTY()
	FRCPresetDescription Description;
};

/**
 * Event triggered when an exposed entity struct is modified.
 */
USTRUCT()
struct FRCPresetEntitiesModifiedEvent
{
	GENERATED_BODY()

	FRCPresetEntitiesModifiedEvent() = default;

	FRCPresetEntitiesModifiedEvent(URemoteControlPreset* InPreset, const TArray<FGuid>& InModifiedEntities)
		: Type(TEXT("PresetEntitiesModified"))
	{
		checkSlow(InPreset);
		PresetName = InPreset->GetFName();
		PresetId = InPreset->GetPresetId().ToString();
		ModifiedEntities = FRCPresetModifiedEntitiesDescription{InPreset, InModifiedEntities};
	}

	/**
	 * Type of the event.
	 */
	UPROPERTY()
	FString Type;

	/**
	 * Name of the preset which contains the modified entities.
	 */
	UPROPERTY()
	FName PresetName;
	
	/**
	 * ID of the preset that contains the modified entities.
	 */
	UPROPERTY()
	FString PresetId;

	/**
	 * The entities that were modified in the last frame.
	 */
	UPROPERTY()
	FRCPresetModifiedEntitiesDescription ModifiedEntities;
};