// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationLibrary.h"
#include "ConversationInstance.h"
#include "ConversationContext.h"
#include "ConversationSettings.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "ConversationLibrary"

//////////////////////////////////////////////////////////////////////////
// UConversationLibrary

UConversationLibrary::UConversationLibrary()
{
}

UConversationInstance* UConversationLibrary::StartConversation(FGameplayTag ConversationEntryTag, AActor* Instigator, FGameplayTag InstigatorTag, AActor* Target, FGameplayTag TargetTag)
{
#if WITH_SERVER_CODE
	if (Instigator == nullptr || Target == nullptr)
	{
		return nullptr;
	}

	if (UWorld* World = GEngine->GetWorldFromContextObject(Instigator, EGetWorldErrorMode::LogAndReturnNull))
	{
		UClass* InstanceClass = GetDefault<UConversationSettings>()->GetConversationInstanceClass();
		if (InstanceClass == nullptr)
		{
			InstanceClass = UConversationInstance::StaticClass();
		}

		UConversationInstance* ConversationInstance = NewObject<UConversationInstance>(World, InstanceClass);
		if (ensure(ConversationInstance))
		{
			FConversationContext Context = FConversationContext::CreateServerContext(ConversationInstance, nullptr);

			UConversationContextHelpers::MakeConversationParticipant(Context, Target, TargetTag);
			UConversationContextHelpers::MakeConversationParticipant(Context, Instigator, InstigatorTag);

			ConversationInstance->ServerStartConversation(ConversationEntryTag);
		}

		return ConversationInstance;
	}
#endif

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
