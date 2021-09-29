// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FGroomAnimationInfo;
struct FHairImportContext;
class IGroomTranslator;
class UGroomAsset;
class UGroomCache;

struct HAIRSTRANDSEDITOR_API FGroomCacheImporter
{
	static TArray<UGroomCache*> ImportGroomCache(const FString& SourceFilename, TSharedPtr<IGroomTranslator> Translator, const FGroomAnimationInfo& InAnimInfo, FHairImportContext& HairImportContext, UGroomAsset* GroomAssetForCache);
};
